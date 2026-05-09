#include "gb_cartridge.h"
#include <cstring>

Cartridge::Cartridge() {
    rom.resize(0x8000);
    ram.resize(0x8000);
}

static u8 read_file(const std::string& path, std::vector<u8>& dst) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    dst.resize(sz < 0x8000 ? 0x8000 : (size_t)sz);
    fread(dst.data(), 1, dst.size(), f);
    fclose(f);
    return 0;
}

bool Cartridge::load(const std::string& path) {
    if (read_file(path, rom) != 0) return false;

    u8 cart_type = rom[0x147];
    rom_size = rom[0x148];
    u8 ram_size = rom[0x149];

    num_rom_banks = rom_size <= 8 ? (2 << rom_size) : 128;

    switch (cart_type) {
        case 0x00: mbc_type = MBC_NONE; break;
        case 0x01: case 0x02: case 0x03: mbc_type = MBC1; break;
        case 0x05: case 0x06: mbc_type = MBC2; break;
        case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: mbc_type = MBC3; break;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: mbc_type = MBC5; break;
        default: mbc_type = MBC_NONE; break;
    }

    switch (ram_size) {
        case 0: num_ram_banks = 0; break;
        case 1: num_ram_banks = 1; break;
        case 2: num_ram_banks = 1; break;
        case 3: num_ram_banks = 4; break;
        case 4: num_ram_banks = 16; break;
        case 5: num_ram_banks = 8; break;
    }

    if (num_ram_banks > 0) ram.resize(num_ram_banks * 0x2000, 0);
    else ram.resize(0x2000, 0);

    return true;
}

u8 Cartridge::read(u16 addr) {
    if (addr < 0x4000) {
        return rom[addr];
    }
    if (addr < 0x8000) {
        u32 bank = rom_bank;
        if (mbc_type == MBC1 && mbc1_mode == 0 && rom_size >= 5) bank &= ~0x20;
        u32 real_bank = bank % num_rom_banks;
        if (real_bank == 0) real_bank = 1;
        u32 offset = (u32)(addr - 0x4000) + real_bank * 0x4000;
        if (offset >= rom.size()) offset %= rom.size();
        return rom[offset];
    }
    if (addr >= 0xA000 && addr < 0xC000) {
        if (!ram_enable) return 0xFF;
        if (mbc_type == MBC1 && mbc1_mode == 1) {
            u32 bank = ram_bank;
            if (num_ram_banks > 0) bank %= num_ram_banks;
            if (bank == 0 && num_ram_banks <= 1) {
                u32 offset = (u32)(addr - 0xA000);
                return offset < ram.size() ? ram[offset] : 0xFF;
            }
            u32 offset = (u32)(addr - 0xA000) + bank * 0x2000;
            return offset < ram.size() ? ram[offset] : 0xFF;
        }
        u32 offset = (u32)(addr - 0xA000);
        return offset < ram.size() ? ram[offset] : 0xFF;
    }
    return 0xFF;
}

void Cartridge::write(u16 addr, u8 val) {
    if (addr < 0x2000) {
        if (mbc_type == MBC1) {
            ram_enable = ((val & 0x0F) == 0x0A);
        } else if (mbc_type == MBC3) {
            ram_enable = ((val & 0x0F) == 0x0A);
        } else if (mbc_type == MBC5) {
            ram_enable = ((val & 0x0F) == 0x0A);
        } else if (mbc_type == MBC2) {
            if ((addr & 0x0100) == 0) ram_enable = ((val & 0x0F) == 0x0A);
        }
    } else if (addr < 0x4000) {
        if (mbc_type == MBC1) {
            u8 bank = val & 0x1F;
            if (bank == 0) bank = 1;
            rom_bank = (rom_bank & 0x60) | bank;
        } else if (mbc_type == MBC3) {
            u8 bank = val & 0x7F;
            if (bank == 0) bank = 1;
            rom_bank = bank;
        } else if (mbc_type == MBC5) {
            if (addr < 0x3000) rom_bank = (rom_bank & 0x100) | val;
            else rom_bank = (rom_bank & 0xFF) | ((val & 1) << 8);
        } else if (mbc_type == MBC2) {
            if ((addr & 0x0100) != 0) {
                u8 bank = val & 0x0F;
                if (bank == 0) bank = 1;
                rom_bank = bank;
            }
        }
    } else if (addr < 0x6000) {
        if (mbc_type == MBC1) {
            if (mbc1_mode == 0) {
                u8 bank = val & 3;
                rom_bank = (rom_bank & 0x1F) | (bank << 5);
            } else {
                ram_bank = val & 3;
            }
        } else if (mbc_type == MBC3) {
            ram_bank = val & 0x03;
        } else if (mbc_type == MBC5) {
            ram_bank = val & 0x0F;
        }
    } else if (addr < 0x8000) {
        if (mbc_type == MBC1) {
            mbc1_mode = val & 1;
        }
    } else if (addr >= 0xA000 && addr < 0xC000) {
        if (!ram_enable) return;
        u32 offset = (u32)(addr - 0xA000);
        if (mbc_type == MBC1 && mbc1_mode == 1 && num_ram_banks > 1) {
            offset += ram_bank * 0x2000;
        }
        if (offset < ram.size()) ram[offset] = val;
    }
}
