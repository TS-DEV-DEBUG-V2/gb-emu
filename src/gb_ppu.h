#pragma once
#include "gb_types.h"
#include "gb_memory.h"
#include <array>

struct PPU {
    std::array<u32, SCREEN_W * SCREEN_H> framebuffer;
    std::array<u32, SCREEN_W * SCREEN_H> pixels;

    u32 dot_counter = 0;
    PPUMode mode = PPU_MODE_OAM_SCAN;
    u32 frame_count = 0;
    bool lcd_was_enabled = false;
    int mode3_duration = 0;

    struct SpriteInfo {
        u8 x, tile, flags;
        int y;
        u8 index;
    };
    SpriteInfo visible_sprites[10];
    int num_sprites = 0;

    u8 bg_tile_cache[21][8];
    u8 win_tile_cache[21][8];

    void reset();
    void step(u8 cycles, Memory& mem, u8* iflag);
    bool dma_transfer(u8 val, Memory& mem, u8* iflag);

private:
    u32 get_color(u8 palette, u8 color_idx) const;
    int decode_tile_row(u8 byte1, u8 byte2, u8* out);
    void render_line(Memory& mem, u8 ly);
    void scan_oam(Memory& mem, u8 ly, int sprite_h);
    void update_stat(Memory& mem, u8* iflag);
};
