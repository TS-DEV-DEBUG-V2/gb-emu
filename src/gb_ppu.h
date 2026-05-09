#pragma once
#include "gb_types.h"
#include "gb_memory.h"
#include <array>

struct PPU {
    std::array<u32, SCREEN_W * SCREEN_H> framebuffer;
    std::array<u32, SCREEN_W * SCREEN_H> pixels;
    u32 cycle_count = 0;
    u8 line_cycles = 0;
    PPUMode mode = PPU_MODE_OAM_SCAN;
    u8 stat_interrupt_line = 0;
    u32 frame_count = 0;
    bool lcd_was_enabled = false;

    void reset();
    void step(u8 cycles, Memory& mem, u8* iflag);
    bool dma_transfer(u8 val, Memory& mem, u8* iflag);

private:
    u8 read_tile_byte(Memory& mem, u16 base, u8 tile, u8 row, bool signed_mode);
    u32 get_color(u8 palette, u8 color_idx);
    void render_line(Memory& mem);
    void update_stat(Memory& mem, u8* iflag);
};
