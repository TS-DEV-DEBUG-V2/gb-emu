#pragma once
#include "gb_types.h"
#include "gb_cartridge.h"
#include <array>

struct Input;
struct APU;

struct Memory {
    Cartridge* cart = nullptr;
    Input* input = nullptr;
    APU* apu = nullptr;
    std::array<u8, 0x2000> vram;
    std::array<u8, 0x2000> wram;
    std::array<u8, 0xA0> oam;
    std::array<u8, 0x80> hram;
    std::array<u8, 0x80> io_regs;
    u8 ie_register = 0;

    Memory();
    void reset();
    void set_cartridge(Cartridge* c) { cart = c; }
    void set_input(Input* i) { input = i; }
    void set_apu(APU* a) { apu = a; }
    u8 read(u16 addr);
    void write(u16 addr, u8 val);
    u16 read16(u16 addr);
    void write16(u16 addr, u16 val);
};