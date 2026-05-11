#pragma once
#include "gb_types.h"
#include "gb_memory.h"
#include "gb_ppu.h"
#include "gb_timer.h"
#include "gb_input.h"
#include "gb_apu.h"

struct CPU {
    union {
        struct { u8 f, a; };
        u16 af;
    };
    union {
        struct { u8 c, b; };
        u16 bc;
    };
    union {
        struct { u8 e, d; };
        u16 de;
    };
    union {
        struct { u8 l, h; };
        u16 hl;
    };
    u16 sp;
    u16 pc;
    bool halted = false;
    bool stop_mode = false;
    bool ime = false;
    u8 ime_scheduled = 0;

    void reset();
    u8 tick(Memory& mem, PPU& ppu, Timer& timer, Input& input, APU& apu);
};

void set_z(CPU& cpu, bool v);
void set_n(CPU& cpu, bool v);
void set_h(CPU& cpu, bool v);
void set_c(CPU& cpu, bool v);
bool get_z(CPU& cpu);
bool get_n(CPU& cpu);
bool get_h(CPU& cpu);
bool get_c(CPU& cpu);

static inline u8 read_imm8(CPU& cpu, Memory& mem) {
    return mem.read(cpu.pc++);
}

static inline u16 read_imm16(CPU& cpu, Memory& mem) {
    u16 lo = mem.read(cpu.pc++);
    u16 hi = mem.read(cpu.pc++);
    return lo | (hi << 8);
}
