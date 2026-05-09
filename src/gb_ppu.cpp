#include "gb_ppu.h"
#include <cstring>
#include <algorithm>

static const u32 colors[4] = {
    0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000
};

void PPU::reset() {
    framebuffer.fill(0xFFFFFFFF);
    pixels.fill(0xFFFFFFFF);
    cycle_count = 0;
    line_cycles = 0;
    mode = PPU_MODE_OAM_SCAN;
    stat_interrupt_line = 0;
    frame_count = 0;
}

u32 PPU::get_color(u8 palette, u8 color_idx) {
    u8 shift = color_idx * 2;
    u8 idx = (palette >> shift) & 3;
    return colors[idx];
}

void PPU::render_line(Memory& mem) {
    u8 lcdc = mem.read(LCDC);
    u8 ly = mem.read(LY);
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

    int y = ly;
    int sprite_h = obj_size ? 16 : 8;

    struct SpriteInfo {
        u8 x, tile, flags;
        int y;
    };
    SpriteInfo visible_sprites[10];
    int num_sprites = 0;

    if (obj_enable) {
        for (int i = 0; i < 40 && num_sprites < 10; i++) {
            u16 oam_addr = 0xFE00 + i * 4;
            u8 sy = mem.read(oam_addr);
            u8 sx = mem.read(oam_addr + 1);
            u8 stile = mem.read(oam_addr + 2);
            u8 sflags = mem.read(oam_addr + 3);
            if (sy == 0 || sy >= 160) continue;
            int spr_y = (int)sy - 16;
            if (y >= spr_y && y < spr_y + sprite_h) {
                visible_sprites[num_sprites++] = {sx, stile, sflags, spr_y};
            }
        }

        std::sort(visible_sprites, visible_sprites + num_sprites,
            [](const SpriteInfo& a, const SpriteInfo& b) {
                return a.x < b.x;
            });
    }

    for (int x = 0; x < SCREEN_W; x++) {
        u32 color = 0xFFFFFFFF;
        u8 bg_color_idx = 0;

        if (bg_enable) {
            int bg_x = (x + scx) & 255;
            int bg_y = (y + scy) & 255;
            int tile_x = bg_x / 8;
            int tile_y = bg_y / 8;
            int pixel_x = bg_x % 8;
            int pixel_y = bg_y % 8;
            u16 map_addr = bg_map_base + tile_y * 32 + tile_x;
            u8 tile_num = mem.read(map_addr);

            u16 tile_addr;
            if (signed_tiles) {
                tile_addr = tile_data_base + ((s8)tile_num + 128) * 16 + pixel_y * 2;
            } else {
                tile_addr = tile_data_base + (u16)tile_num * 16 + pixel_y * 2;
            }
            u8 byte1 = mem.read(tile_addr);
            u8 byte2 = mem.read(tile_addr + 1);
            bg_color_idx = ((byte2 >> (7 - pixel_x)) & 1) << 1 | ((byte1 >> (7 - pixel_x)) & 1);
            color = get_color(bgp, bg_color_idx);
        }

        if (win_enable && wy <= y && wx < 168) {
            int win_x = (int)wx - 7;
            if (win_x < 0) win_x = 0;
            int win_draw_x = x - win_x;
            int win_y = y - wy;
            if (win_draw_x >= 0 && win_y >= 0) {
                int tile_x_w = win_draw_x / 8;
                int tile_y_w = win_y / 8;
                int pixel_x_w = win_draw_x % 8;
                int pixel_y_w = win_y % 8;
                u16 map_addr_w = win_map_base + tile_y_w * 32 + tile_x_w;
                u8 tile_num_w = mem.read(map_addr_w);

                u16 tile_addr_w;
                if (signed_tiles) {
                    tile_addr_w = tile_data_base + ((s8)tile_num_w + 128) * 16 + pixel_y_w * 2;
                } else {
                    tile_addr_w = tile_data_base + (u16)tile_num_w * 16 + pixel_y_w * 2;
                }
                u8 byte1w = mem.read(tile_addr_w);
                u8 byte2w = mem.read(tile_addr_w + 1);
                u8 color_idx_w = ((byte2w >> (7 - pixel_x_w)) & 1) << 1 | ((byte1w >> (7 - pixel_x_w)) & 1);
                if (color_idx_w != 0) {
                    bg_color_idx = color_idx_w;
                    color = get_color(bgp, color_idx_w);
                }
            }
        }

        u8 final_color_idx = bg_color_idx;
        if (obj_enable) {
            for (int si = num_sprites - 1; si >= 0; si--) {
                auto& s = visible_sprites[si];
                int spr_x = (int)s.x - 8;
                if (x < spr_x || x >= spr_x + 8) continue;

                int pixel_x_s = x - spr_x;
                int pixel_y_s = y - s.y;

                bool x_flip = s.flags & 0x20;
                bool y_flip = s.flags & 0x40;
                bool bg_priority = s.flags & 0x80;
                u8 obj_palette = (s.flags & 0x10) ? obp1 : obp0;

                if (y_flip) pixel_y_s = sprite_h - 1 - pixel_y_s;
                if (x_flip) pixel_x_s = 7 - pixel_x_s;

                u8 tile_num_s = s.tile;
                if (obj_size) tile_num_s &= 0xFE;

                u16 tile_addr = 0x8000 + (u16)tile_num_s * 16 + pixel_y_s * 2;
                u8 byte1s = mem.read(tile_addr);
                u8 byte2s = mem.read(tile_addr + 1);
                u8 color_idx_s = ((byte2s >> (7 - pixel_x_s)) & 1) << 1 | ((byte1s >> (7 - pixel_x_s)) & 1);

                if (color_idx_s == 0) continue;

                if (bg_priority && bg_color_idx != 0) continue;

                color = get_color(obj_palette, color_idx_s);
                break;
            }
        }

        framebuffer[y * SCREEN_W + x] = color;
    }
}

void PPU::update_stat(Memory& mem, u8* iflag) {
    u8 stat = mem.read(STAT);
    stat = (stat & 0xFC) | (mode & 3);
    bool lyc_eq = (mem.read(LY) == mem.read(LYC));
    if (lyc_eq) stat |= 0x04;
    else stat &= ~0x04;

    bool old_lyc_if = stat_interrupt_line;
    stat_interrupt_line = 0;

    if ((stat & 0x40) && mode == PPU_MODE_VBLANK) stat_interrupt_line = 1;
    if ((stat & 0x20) && mode == PPU_MODE_OAM_SCAN) stat_interrupt_line = 1;
    if ((stat & 0x10) && mode == PPU_MODE_HBLANK) stat_interrupt_line = 1;
    if ((stat & 0x08) && lyc_eq) stat_interrupt_line = 1;

    mem.write(STAT, stat);

    if (stat_interrupt_line && !old_lyc_if) {
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
    bool lcd_enable = lcdc & 0x80;

    if (!lcd_enable) {
        lcd_was_enabled = false;
        mode = PPU_MODE_HBLANK;
        mem.write(LY, 0);
        cycle_count = 0;
        return;
    }

    if (!lcd_was_enabled) {
        lcd_was_enabled = true;
        mode = PPU_MODE_OAM_SCAN;
        cycle_count = 0;
        mem.write(LY, 0);
    }

    u8 ly = mem.read(LY);
    cycle_count += cycles;

    int mode_cycles = 0;
    if (mode == PPU_MODE_OAM_SCAN) mode_cycles = 80;
    else if (mode == PPU_MODE_DRAW) mode_cycles = 172;
    else if (mode == PPU_MODE_HBLANK) mode_cycles = 204;
    else mode_cycles = 456;

    if (cycle_count >= mode_cycles) {
        cycle_count -= mode_cycles;

        if (mode == PPU_MODE_OAM_SCAN) {
            mode = PPU_MODE_DRAW;
            render_line(mem);
        } else if (mode == PPU_MODE_DRAW) {
            mode = PPU_MODE_HBLANK;
            update_stat(mem, iflag);
        } else if (mode == PPU_MODE_HBLANK) {
            ly++;
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
        } else if (mode == PPU_MODE_VBLANK) {
            ly++;
            mem.write(LY, ly);
            if (ly > 153) {
                ly = 0;
                mem.write(LY, ly);
                mode = PPU_MODE_OAM_SCAN;
            }
            update_stat(mem, iflag);
        }
    }
}
