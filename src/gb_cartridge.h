#pragma once
#include "gb_types.h"
#include <vector>
#include <string>
#include <cstdio>

struct Cartridge {
    std::vector<u8> rom;
    std::vector<u8> ram;
    MBCType mbc_type = MBC_NONE;
    u8 rom_bank = 1;
    u8 ram_bank = 0;
    bool ram_enable = false;
    bool ram_enable_inv = false;
    int num_rom_banks = 2;
    int num_ram_banks = 0;
    u8 mbc1_mode = 0;
    u8 mbc1_shift = 0;
    u8 rom_size = 0;

    Cartridge();
    bool load(const std::string& path);
    u8 read(u16 addr);
    void write(u16 addr, u8 val);
};
