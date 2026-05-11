#include "gb_ppu.h"
#include <cstring>
#include <algorithm>

static const u32 colors[4] = {
    0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000
};

static const int OAM_CYCLES = 80;
static const int MODE3_BASE = 172;
static const int MODE3_PER_SPRITE = 8;
static const int CYCLES_PER_LINE = 456;

void PPU::reset() {
    framebuffer.fill(0xFFFFFFFF);
    pixels.fill(0xFFFFFFFF);
    dot_counter = 0;
    mode = PPU_MODE_OAM_SCAN;
    frame_count = 0;
    lcd_was_enabled = false;
    num_sprites = 0;
    mode3_duration = 0;
}

u32 PPU::get_color(u8 palette, u8 color_idx) const {
    u8 shift = color_idx * 2;
    u8 idx = (palette >> shift) & 3;
    return colors[idx];
}

int PPU::decode_tile_row(u8 byte1, u8 byte2, u8* out) {
    out[0] = ((byte2 >> 7) & 1) << 1 | ((byte1 >> 7) & 1);
    out[1] = ((byte2 >> 6) & 1) << 1 | ((byte1 >> 6) & 1);
    out[2] = ((byte2 >> 5) & 1) << 1 | ((byte1 >> 5) & 1);
    out[3] = ((byte2 >> 4) & 1) << 1 | ((byte1 >> 4) & 1);
    out[4] = ((byte2 >> 3) & 1) << 1 | ((byte1 >> 3) & 1);
    out[5] = ((byte2 >> 2) & 1) << 1 | ((byte1 >> 2) & 1);
    out[6] = ((byte2 >> 1) & 1) << 1 | ((byte1 >> 1) & 1);
    out[7] = (byte2 & 1) << 1 | (byte1 & 1);
    return 0;
}

void PPU::scan_oam(Memory& mem, u8 ly, int sprite_h) {
    num_sprites = 0;
    for (int i = 0; i < 40 && num_sprites < 10; i++) {
        u16 oam_addr = 0xFE00 + i * 4;
        u8 sy = mem.read(oam_addr);
        u8 sx = mem.read(oam_addr + 1);
        u8 stile = mem.read(oam_addr + 2);
        u8 sflags = mem.read(oam_addr + 3);
        if (sy == 0 || sy >= 160) continue;
        int spr_y = (int)sy - 16;
        if (ly >= spr_y && ly < spr_y + sprite_h) {
            SpriteInfo si;
            si.x = sx; si.tile = stile; si.flags = sflags;
            si.y = spr_y; si.index = (u8)i;
            visible_sprites[num_sprites++] = si;
        }
    }

    std::stable_sort(visible_sprites, visible_sprites + num_sprites,
        [](const SpriteInfo& a, const SpriteInfo& b) {
            if (a.x != b.x) return a.x < b.x;
            return a.index < b.index;
        });
}

void PPU::render_line(Memory& mem, u8 ly) {
    u8 lcdc = mem.read(LCDC);
    u8 scx = mem.read(SCX);
    u8 scy = mem.read(SCY);
    u8 bgp = mem.read(BGP);
    u8 obp0 = mem.read(OBP0);
    u8 obp1 = mem.read(OBP1);
    u8 wy = mem.read(WY);
    u8 wx = mem.read(WX);

    bool bg_enable = lcdc & 0x01;
    bool obj_enable = lcdc & 0x02;
    bool obj_size = lcdc & 0x04;
    bool bg_tile_map = lcdc & 0x08;
    bool bg_tile_data = lcdc & 0x10;
    bool win_enable = lcdc & 0x20;
    bool win_tile_map = lcdc & 0x40;
    bool lcd_enable = lcdc & 0x80;

    if (!lcd_enable) return;

    u16 bg_map_base = bg_tile_map ? 0x9C00 : 0x9800;
    u16 win_map_base = win_tile_map ? 0x9C00 : 0x9800;
    u16 tile_data_base = bg_tile_data ? 0x8000 : 0x8800;
    bool signed_tiles = !bg_tile_data;
    int sprite_h = obj_size ? 16 : 8;

    scan_oam(mem, ly, sprite_h);

    u8 bg_pixels[SCREEN_W];
    u8 win_pixels[SCREEN_W];
    bool win_active[SCREEN_W];
    std::memset(bg_pixels, 0, SCREEN_W);
    std::memset(win_pixels, 0, SCREEN_W);
    std::memset(win_active, 0, SCREEN_W);

    int bg_tile_y = ((int)ly + (int)scy) & 255;
    int bg_tile_row = bg_tile_y / 8;
    int bg_pixel_row = bg_tile_y % 8;

    int win_tile_row = ((int)ly - (int)wy) / 8;
    int win_pixel_row = ((int)ly - (int)wy) % 8;
    int win_x_offset = (int)wx - 7;

    u16 bg_map_row = bg_map_base + bg_tile_row * 32;

    if (bg_enable) {
        int first_tile_x = (scx >> 3) & 31;
        for (int t = 0; t < 21; t++) {
            int map_col = (first_tile_x + t) & 31;
            u8 tile_num = mem.read(bg_map_row + map_col);

            u16 tile_addr;
            if (signed_tiles)
                tile_addr = tile_data_base + ((s8)tile_num + 128) * 16 + bg_pixel_row * 2;
            else
                tile_addr = tile_data_base + (u16)tile_num * 16 + bg_pixel_row * 2;

            u8 byte1 = mem.read(tile_addr);
            u8 byte2 = mem.read(tile_addr + 1);
            decode_tile_row(byte1, byte2, bg_tile_cache[t]);
        }

        for (int x = 0; x < SCREEN_W; x++) {
            int bg_x = (x + scx) & 255;
            int tile_x = bg_x >> 3;
            int pixel_x = bg_x & 7;
            int cache_idx = (tile_x - first_tile_x) & 31;
            bg_pixels[x] = bg_tile_cache[cache_idx][pixel_x];
        }
    }

    if (win_enable && ly >= wy && wx <= 166) {
        if (win_x_offset < 0) win_x_offset = 0;
        u16 win_map_row = win_map_base + win_tile_row * 32;

        for (int t = 0; t < 21; t++) {
            int map_col = t & 31;
            u8 tile_num_w = mem.read(win_map_row + map_col);

            u16 tile_addr_w;
            if (signed_tiles)
                tile_addr_w = tile_data_base + ((s8)tile_num_w + 128) * 16 + win_pixel_row * 2;
            else
                tile_addr_w = tile_data_base + (u16)tile_num_w * 16 + win_pixel_row * 2;

            u8 byte1w = mem.read(tile_addr_w);
            u8 byte2w = mem.read(tile_addr_w + 1);
            decode_tile_row(byte1w, byte2w, win_tile_cache[t]);
        }

        for (int x = 0; x < SCREEN_W; x++) {
            int win_draw_x = x - win_x_offset;
            if (win_draw_x >= 0) {
                int tile_x_w = win_draw_x >> 3;
                int pixel_x_w = win_draw_x & 7;
                u8 ci = win_tile_cache[tile_x_w][pixel_x_w];
                win_active[x] = true;
                win_pixels[x] = ci;
            }
        }
    }

    u8 sprite_pixels[10][8];
    for (int si = 0; si < num_sprites; si++) {
        auto& s = visible_sprites[si];
        int pixel_y_s = (int)ly - s.y;
        bool y_flip = s.flags & 0x40;
        if (y_flip) pixel_y_s = sprite_h - 1 - pixel_y_s;

        u8 tile_num_s = s.tile;
        if (obj_size) tile_num_s &= 0xFE;

        u16 tile_addr = 0x8000 + (u16)tile_num_s * 16 + pixel_y_s * 2;
        u8 byte1s = mem.read(tile_addr);
        u8 byte2s = mem.read(tile_addr + 1);
        decode_tile_row(byte1s, byte2s, sprite_pixels[si]);

        bool x_flip = s.flags & 0x20;
        if (x_flip) {
            for (int i = 0; i < 4; i++) {
                std::swap(sprite_pixels[si][i], sprite_pixels[si][7 - i]);
            }
        }
    }

    for (int x = 0; x < SCREEN_W; x++) {
        u8 color_idx = bg_pixels[x];
        u8 bg_px = bg_pixels[x];

        if (win_active[x]) {
            color_idx = win_pixels[x];
            bg_px = win_pixels[x];
        }

        u8 bg_priority_idx = bg_px;

        if (obj_enable) {
            for (int si = 0; si < num_sprites; si++) {
                auto& s = visible_sprites[si];
                int spr_x = (int)s.x - 8;
                if (x < spr_x || x >= spr_x + 8) continue;

                int pixel_x_s = x - spr_x;
                u8 ci = sprite_pixels[si][pixel_x_s];
                if (ci == 0) continue;

                bool bg_priority = s.flags & 0x80;
                if (bg_priority && bg_priority_idx != 0) continue;

                u8 obj_palette = (s.flags & 0x10) ? obp1 : obp0;
                color_idx = ci;
                framebuffer[ly * SCREEN_W + x] = get_color(obj_palette, color_idx);
                goto next_pixel;
            }
        }

        framebuffer[ly * SCREEN_W + x] = get_color(bgp, color_idx);
next_pixel:;
    }
}

void PPU::update_stat(Memory& mem, u8* iflag) {
    u8 stat = mem.read(STAT);
    stat = (stat & 0xFC) | (mode & 3);

    u8 ly = mem.read(LY);
    u8 lyc = mem.read(LYC);
    bool lyc_eq = (ly == lyc);
    if (lyc_eq) stat |= 0x04;
    else stat &= ~0x04;

    bool old_stat_if = (mem.io_regs[0x0F] & INT_STAT) != 0;

    u8 stat_interrupt = 0;
    if ((stat & 0x40) && mode == PPU_MODE_VBLANK) stat_interrupt = 1;
    if ((stat & 0x20) && mode == PPU_MODE_OAM_SCAN) stat_interrupt = 1;
    if ((stat & 0x10) && mode == PPU_MODE_HBLANK) stat_interrupt = 1;
    if ((stat & 0x08) && lyc_eq) stat_interrupt = 1;

    mem.write(STAT, stat);

    if (stat_interrupt && !old_stat_if) {
        *iflag |= INT_STAT;
    }
}

bool PPU::dma_transfer(u8 val, Memory& mem, u8* iflag) {
    u16 src = (u16)val << 8;
    for (int i = 0; i < 0xA0; i++) {
        mem.write(0xFE00 + i, mem.read(src + i));
    }
    return true;
}

void PPU::step(u8 cycles, Memory& mem, u8* iflag) {
    u8 lcdc = mem.read(LCDC);
    bool lcd_ena = lcdc & 0x80;

    if (!lcd_ena) {
        if (lcd_was_enabled) {
            lcd_was_enabled = false;
            mem.write(LY, 0);
            mode = PPU_MODE_HBLANK;
            dot_counter = 0;
            num_sprites = 0;
            framebuffer.fill(0xFFFFFFFF);
        }
        return;
    }

    if (!lcd_was_enabled) {
        lcd_was_enabled = true;
        mode = PPU_MODE_OAM_SCAN;
        dot_counter = 0;
        mem.write(LY, 0);
    }

    dot_counter += cycles;

    while (true) {
        switch (mode) {
            case PPU_MODE_OAM_SCAN: {
                if (dot_counter < OAM_CYCLES) return;
                dot_counter -= OAM_CYCLES;
                mode = PPU_MODE_DRAW;
                render_line(mem, mem.read(LY));
                int ns = num_sprites > 10 ? 10 : num_sprites;
                mode3_duration = MODE3_BASE + ns * MODE3_PER_SPRITE;
                if (mode3_duration > 289) mode3_duration = 289;
                update_stat(mem, iflag);
                continue;
            }
            case PPU_MODE_DRAW: {
                if (dot_counter < (u32)mode3_duration) return;
                dot_counter -= mode3_duration;
                mode = PPU_MODE_HBLANK;
                update_stat(mem, iflag);
                continue;
            }
            case PPU_MODE_HBLANK: {
                int hblank_dur = CYCLES_PER_LINE - OAM_CYCLES - mode3_duration;
                if (dot_counter < (u32)hblank_dur) return;
                dot_counter -= hblank_dur;
                u8 ly = mem.read(LY) + 1;
                mem.write(LY, ly);
                if (ly >= SCREEN_H) {
                    mode = PPU_MODE_VBLANK;
                    *iflag |= INT_VBLANK;
                    frame_count++;
                    std::memcpy(pixels.data(), framebuffer.data(), framebuffer.size() * sizeof(u32));
                } else {
                    mode = PPU_MODE_OAM_SCAN;
                }
                update_stat(mem, iflag);
                continue;
            }
            case PPU_MODE_VBLANK: {
                if (dot_counter < CYCLES_PER_LINE) return;
                dot_counter -= CYCLES_PER_LINE;
                u8 ly = mem.read(LY) + 1;
                if (ly > 153) {
                    mem.write(LY, 0);
                    mode = PPU_MODE_OAM_SCAN;
                } else {
                    mem.write(LY, ly);
                }
                update_stat(mem, iflag);
                continue;
            }
        }
    }
}
