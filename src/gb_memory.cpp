#include "gb_memory.h"
#include "gb_input.h"
#include "gb_apu.h"
#include <cstring>

Memory::Memory() {
    reset();
}

void Memory::reset() {
    vram.fill(0);
    wram.fill(0);
    oam.fill(0);
    hram.fill(0);
    io_regs.fill(0);
    ie_register = 0;

    io_regs[NR10 - 0xFF00] = 0x80;
    io_regs[NR11 - 0xFF00] = 0xBF;
    io_regs[NR12 - 0xFF00] = 0xF3;
    io_regs[NR14 - 0xFF00] = 0xBF;
    io_regs[NR21 - 0xFF00] = 0x3F;
    io_regs[NR24 - 0xFF00] = 0xBF;
    io_regs[NR30 - 0xFF00] = 0x7F;
    io_regs[NR31 - 0xFF00] = 0xFF;
    io_regs[NR32 - 0xFF00] = 0x9F;
    io_regs[NR34 - 0xFF00] = 0xBF;
    io_regs[NR41 - 0xFF00] = 0xFF;
    io_regs[NR44 - 0xFF00] = 0xBF;
    io_regs[NR50 - 0xFF00] = 0x77;
    io_regs[NR51 - 0xFF00] = 0xF3;
    io_regs[NR52 - 0xFF00] = 0xF1;
}

u8 Memory::read(u16 addr) {
    if (addr < 0x8000) {
        if (cart) return cart->read(addr);
        return 0xFF;
    }
    if (addr < 0xA000) return vram[addr & 0x1FFF];
    if (addr < 0xC000) {
        if (cart) return cart->read(addr);
        return 0xFF;
    }
    if (addr < 0xE000) return wram[addr & 0x1FFF];
    if (addr < 0xFE00) return wram[addr & 0x1FFF];
    if (addr < 0xFEA0) return oam[addr & 0xFF];
    if (addr == 0xFF00) {
        if (input) return input->read_p1();
        return 0xFF;
    }
    if (addr >= 0xFF04 && addr <= 0xFF07) return io_regs[addr - 0xFF00];
    if (addr == 0xFF0F) return io_regs[0x0F];
    if (addr >= 0xFF10 && addr <= 0xFF3F) return io_regs[addr - 0xFF00];
    if (addr >= 0xFF40 && addr <= 0xFF4B) return io_regs[addr - 0xFF00];
    if (addr >= 0xFF4F && addr <= 0xFF53) return io_regs[addr - 0xFF00];
    if (addr >= 0xFF68 && addr <= 0xFF6F) return io_regs[addr - 0xFF00];
    if (addr >= 0xFF70 && addr <= 0xFF7F) return io_regs[addr - 0xFF00];
    if (addr >= 0xFF80 && addr < 0xFFFF) return hram[addr & 0x7F];
    if (addr == 0xFFFF) return ie_register;
    return 0xFF;
}

void Memory::write(u16 addr, u8 val) {
    if (addr < 0x8000) {
        if (cart) cart->write(addr, val);
    } else if (addr < 0xA000) {
        vram[addr & 0x1FFF] = val;
    } else if (addr < 0xC000) {
        if (cart) cart->write(addr, val);
    } else if (addr < 0xE000) {
        wram[addr & 0x1FFF] = val;
    } else if (addr < 0xFE00) {
        wram[addr & 0x1FFF] = val;
    } else if (addr < 0xFEA0) {
        oam[addr & 0xFF] = val;
    } else if (addr >= 0xFF80 && addr < 0xFFFF) {
        hram[addr & 0x7F] = val;
    } else if (addr == 0xFFFF) {
        ie_register = val;
    } else if (addr == 0xFF00) {
        if (input) input->write_p1(val);
    } else if (addr == 0xFF46) {
        u16 src = (u16)val << 8;
        for (int i = 0; i < 0xA0; i++) {
            write(0xFE00 + i, read(src + i));
        }
    } else if (addr == DIV) {
        io_regs[addr - 0xFF00] = 0;
    } else if (addr >= 0xFF10 && addr <= 0xFF3F) {
        io_regs[addr - 0xFF00] = val;
        if (apu) apu->write_reg(addr, val);
    } else if (addr >= 0xFF00 && addr < 0xFF80) {
        io_regs[addr - 0xFF00] = val;
    }
}

u16 Memory::read16(u16 addr) {
    return read(addr) | (u16)(read(addr + 1) << 8);
}

void Memory::write16(u16 addr, u16 val) {
    write(addr, val & 0xFF);
    write(addr + 1, (val >> 8) & 0xFF);
}