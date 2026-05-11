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
    mutable bool dirty = false;

    Cartridge();
    bool load(const std::string& path);
    bool save_ram(const std::string& path) const;
    bool load_ram(const std::string& path);
    u8 read(u16 addr);
    void write(u16 addr, u8 val);
    size_t ram_size() const { return num_ram_banks > 0 ? (size_t)num_ram_banks * 0x2000 : 0x2000; }
    const u8* ram_data() const { return ram.data(); }
    u8* ram_data() { return ram.data(); }
};
