#include "gb_cpu.h"
#include <cstring>

void set_z(CPU& cpu, bool v) { cpu.f = (cpu.f & ~0x80) | (v ? 0x80 : 0); }
void set_n(CPU& cpu, bool v) { cpu.f = (cpu.f & ~0x40) | (v ? 0x40 : 0); }
void set_h(CPU& cpu, bool v) { cpu.f = (cpu.f & ~0x20) | (v ? 0x20 : 0); }
void set_c(CPU& cpu, bool v) { cpu.f = (cpu.f & ~0x10) | (v ? 0x10 : 0); }
bool get_z(CPU& cpu) { return (cpu.f & 0x80) != 0; }
bool get_n(CPU& cpu) { return (cpu.f & 0x40) != 0; }
bool get_h(CPU& cpu) { return (cpu.f & 0x20) != 0; }
bool get_c(CPU& cpu) { return (cpu.f & 0x10) != 0; }

static void push(CPU& cpu, Memory& mem, u16 val) {
    cpu.sp -= 2;
    mem.write(cpu.sp, val & 0xFF);
    mem.write(cpu.sp + 1, (val >> 8) & 0xFF);
}

static u16 pop(CPU& cpu, Memory& mem) {
    u16 lo = mem.read(cpu.sp++);
    u16 hi = mem.read(cpu.sp++);
    return lo | (hi << 8);
}

static u8 exec_cb(CPU& cpu, Memory& mem) {
    u8 op = read_imm8(cpu, mem);
    u8 r = op & 7;
    u8 bit = (op >> 3) & 7;
    u8 x = (op >> 6) & 3;

    u8* reg8 = nullptr;
    switch (r) {
        case 0: reg8 = &cpu.b; break;
        case 1: reg8 = &cpu.c; break;
        case 2: reg8 = &cpu.d; break;
        case 3: reg8 = &cpu.e; break;
        case 4: reg8 = &cpu.h; break;
        case 5: reg8 = &cpu.l; break;
        case 6: reg8 = nullptr; break;
        case 7: reg8 = &cpu.a; break;
    }

    u8 val = reg8 ? *reg8 : mem.read(cpu.hl);

    switch (x) {
        case 0: {
            u8 result = val;
            u8 old_c = get_c(cpu) ? 1 : 0;
            u8 new_c = 0;
            u8 new_h = 0;

            switch (bit) {
                case 0: { // RLC
                    new_c = (val >> 7) & 1;
                    result = (val << 1) | new_c;
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
                case 1: { // RRC
                    new_c = val & 1;
                    result = (val >> 1) | (new_c << 7);
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
                case 2: { // RL
                    new_c = (val >> 7) & 1;
                    result = (val << 1) | old_c;
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
                case 3: { // RR
                    new_c = val & 1;
                    result = (val >> 1) | (old_c << 7);
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
                case 4: { // SLA
                    new_c = (val >> 7) & 1;
                    result = val << 1;
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
                case 5: { // SRA
                    new_c = val & 1;
                    result = (val >> 1) | (val & 0x80);
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
                case 6: { // SWAP
                    result = ((val & 0x0F) << 4) | ((val >> 4) & 0x0F);
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, false);
                    break;
                }
                case 7: { // SRL
                    new_c = val & 1;
                    result = val >> 1;
                    set_z(cpu, result == 0);
                    set_n(cpu, false);
                    set_h(cpu, false);
                    set_c(cpu, new_c);
                    break;
                }
            }

            if (reg8) *reg8 = result;
            else mem.write(cpu.hl, result);
            return r == 6 ? 16 : 8;
        }
        case 1: { // BIT
            u8 test = (val >> bit) & 1;
            set_z(cpu, test == 0);
            set_n(cpu, false);
            set_h(cpu, true);
            return r == 6 ? 16 : 8;
        }
        case 2: { // RES
            u8 result = val & ~(1 << bit);
            if (reg8) *reg8 = result;
            else mem.write(cpu.hl, result);
            return r == 6 ? 16 : 8;
        }
        case 3: { // SET
            u8 result = val | (1 << bit);
            if (reg8) *reg8 = result;
            else mem.write(cpu.hl, result);
            return r == 6 ? 16 : 8;
        }
    }
    return 0;
}

void CPU::reset() {
    af = 0x01B0;
    bc = 0x0013;
    de = 0x00D8;
    hl = 0x014D;
    sp = 0xFFFE;
    pc = 0x0100;
    halted = false;
    stop_mode = false;
    ime = false;
    ime_scheduled = 0;
}

u8 CPU::tick(Memory& mem, PPU& ppu, Timer& timer, Input& input, APU& apu) {
    if (ime_scheduled > 0) {
        ime_scheduled--;
        if (ime_scheduled == 0) ime = true;
    }

    u8 iflag = mem.read(IF);
    u8 ie_reg = mem.read(IE);

    if (halted) {
        if ((iflag & ie_reg) != 0) {
            halted = false;
        } else {
            return 4;
        }
    }

    if (ime && (iflag & ie_reg) != 0) {
        ime = false;
        halted = false;
        for (int i = 0; i < 5; i++) {
            if (iflag & (1 << i) & ie_reg) {
                iflag &= ~(1 << i);
                mem.write(IF, iflag);
                push(*this, mem, pc);
                pc = 0x40 + i * 8;
                return 20;
            }
        }
    }

    u8 op = read_imm8(*this, mem);
    u8 cycles = 4;

    switch (op) {
        case 0x00: cycles = 4; break; // NOP
        case 0x01: bc = read_imm16(*this, mem); cycles = 12; break;
        case 0x02: mem.write(bc, a); cycles = 8; break;
        case 0x03: bc++; cycles = 8; break;
        case 0x04: { b++; set_z(*this, b == 0); set_n(*this, false); set_h(*this, (b & 0x0F) == 0); cycles = 4; break; }
        case 0x05: { b--; set_z(*this, b == 0); set_n(*this, true); set_h(*this, (b & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x06: b = read_imm8(*this, mem); cycles = 8; break;
        case 0x07: { u8 c_bit = (a >> 7) & 1; a = (a << 1) | c_bit; set_z(*this, false); set_n(*this, false); set_h(*this, false); set_c(*this, c_bit); cycles = 4; break; }
        case 0x08: { u16 addr = read_imm16(*this, mem); mem.write(addr, sp & 0xFF); mem.write(addr + 1, (sp >> 8) & 0xFF); cycles = 20; break; }
        case 0x09: { u32 tmp = hl + bc; set_n(*this, false); set_h(*this, (hl & 0xFFF) + (bc & 0xFFF) > 0xFFF); set_c(*this, tmp > 0xFFFF); hl = (u16)tmp; cycles = 8; break; }
        case 0x0A: a = mem.read(bc); cycles = 8; break;
        case 0x0B: bc--; cycles = 8; break;
        case 0x0C: { c++; set_z(*this, c == 0); set_n(*this, false); set_h(*this, (c & 0x0F) == 0); cycles = 4; break; }
        case 0x0D: { c--; set_z(*this, c == 0); set_n(*this, true); set_h(*this, (c & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x0E: c = read_imm8(*this, mem); cycles = 8; break;
        case 0x0F: { u8 c_bit = a & 1; a = (a >> 1) | (c_bit << 7); set_z(*this, false); set_n(*this, false); set_h(*this, false); set_c(*this, c_bit); cycles = 4; break; }

        case 0x10: halted = true; cycles = 4; break;
        case 0x11: de = read_imm16(*this, mem); cycles = 12; break;
        case 0x12: mem.write(de, a); cycles = 8; break;
        case 0x13: de++; cycles = 8; break;
        case 0x14: { d++; set_z(*this, d == 0); set_n(*this, false); set_h(*this, (d & 0x0F) == 0); cycles = 4; break; }
        case 0x15: { d--; set_z(*this, d == 0); set_n(*this, true); set_h(*this, (d & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x16: d = read_imm8(*this, mem); cycles = 8; break;
        case 0x17: { u8 c_bit = get_c(*this) ? 1 : 0; u8 new_c = (a >> 7) & 1; a = (a << 1) | c_bit; set_z(*this, false); set_n(*this, false); set_h(*this, false); set_c(*this, new_c); cycles = 4; break; }
        case 0x18: { s8 offset = (s8)read_imm8(*this, mem); pc += offset; cycles = 12; break; }
        case 0x19: { u32 tmp = hl + de; set_n(*this, false); set_h(*this, (hl & 0xFFF) + (de & 0xFFF) > 0xFFF); set_c(*this, tmp > 0xFFFF); hl = (u16)tmp; cycles = 8; break; }
        case 0x1A: a = mem.read(de); cycles = 8; break;
        case 0x1B: de--; cycles = 8; break;
        case 0x1C: { e++; set_z(*this, e == 0); set_n(*this, false); set_h(*this, (e & 0x0F) == 0); cycles = 4; break; }
        case 0x1D: { e--; set_z(*this, e == 0); set_n(*this, true); set_h(*this, (e & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x1E: e = read_imm8(*this, mem); cycles = 8; break;
        case 0x1F: { u8 c_bit = get_c(*this) ? 1 : 0; u8 new_c = a & 1; a = (a >> 1) | (c_bit << 7); set_z(*this, false); set_n(*this, false); set_h(*this, false); set_c(*this, new_c); cycles = 4; break; }

        case 0x20: { s8 offset = (s8)read_imm8(*this, mem); if (!get_z(*this)) { pc += offset; cycles = 12; } else cycles = 8; break; }
        case 0x21: hl = read_imm16(*this, mem); cycles = 12; break;
        case 0x22: { mem.write(hl++, a); cycles = 8; break; }
        case 0x23: hl++; cycles = 8; break;
        case 0x24: { h++; set_z(*this, h == 0); set_n(*this, false); set_h(*this, (h & 0x0F) == 0); cycles = 4; break; }
        case 0x25: { h--; set_z(*this, h == 0); set_n(*this, true); set_h(*this, (h & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x26: h = read_imm8(*this, mem); cycles = 8; break;
        case 0x27: { // DAA
            u16 correction = 0;
            if (get_h(*this) || (!get_n(*this) && (a & 0x0F) > 9)) correction |= 0x06;
            if (get_c(*this) || (!get_n(*this) && a > 0x99)) { correction |= 0x60; set_c(*this, true); }
            a = get_n(*this) ? (a - correction) : (a + correction);
            set_z(*this, a == 0); set_h(*this, false);
            cycles = 4; break;
        }
        case 0x28: { s8 offset = (s8)read_imm8(*this, mem); if (get_z(*this)) { pc += offset; cycles = 12; } else cycles = 8; break; }
        case 0x29: { u32 tmp = hl + hl; set_n(*this, false); set_h(*this, (hl & 0xFFF) + (hl & 0xFFF) > 0xFFF); set_c(*this, tmp > 0xFFFF); hl = (u16)tmp; cycles = 8; break; }
        case 0x2A: { a = mem.read(hl++); cycles = 8; break; }
        case 0x2B: hl--; cycles = 8; break;
        case 0x2C: { l++; set_z(*this, l == 0); set_n(*this, false); set_h(*this, (l & 0x0F) == 0); cycles = 4; break; }
        case 0x2D: { l--; set_z(*this, l == 0); set_n(*this, true); set_h(*this, (l & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x2E: l = read_imm8(*this, mem); cycles = 8; break;
        case 0x2F: { a = ~a; set_n(*this, true); set_h(*this, true); cycles = 4; break; }

        case 0x30: { s8 offset = (s8)read_imm8(*this, mem); if (!get_c(*this)) { pc += offset; cycles = 12; } else cycles = 8; break; }
        case 0x31: sp = read_imm16(*this, mem); cycles = 12; break;
        case 0x32: { mem.write(hl--, a); cycles = 8; break; }
        case 0x33: sp++; cycles = 8; break;
        case 0x34: { u8 val = mem.read(hl); val++; set_z(*this, val == 0); set_n(*this, false); set_h(*this, (val & 0x0F) == 0); mem.write(hl, val); cycles = 12; break; }
        case 0x35: { u8 val = mem.read(hl); val--; set_z(*this, val == 0); set_n(*this, true); set_h(*this, (val & 0x0F) == 0x0F); mem.write(hl, val); cycles = 12; break; }
        case 0x36: mem.write(hl, read_imm8(*this, mem)); cycles = 12; break;
        case 0x37: set_c(*this, true); set_n(*this, false); set_h(*this, false); cycles = 4; break;
        case 0x38: { s8 offset = (s8)read_imm8(*this, mem); if (get_c(*this)) { pc += offset; cycles = 12; } else cycles = 8; break; }
        case 0x39: { u32 tmp = hl + sp; set_n(*this, false); set_h(*this, (hl & 0xFFF) + (sp & 0xFFF) > 0xFFF); set_c(*this, tmp > 0xFFFF); hl = (u16)tmp; cycles = 8; break; }
        case 0x3A: { a = mem.read(hl--); cycles = 8; break; }
        case 0x3B: sp--; cycles = 8; break;
        case 0x3C: { a++; set_z(*this, a == 0); set_n(*this, false); set_h(*this, (a & 0x0F) == 0); cycles = 4; break; }
        case 0x3D: { a--; set_z(*this, a == 0); set_n(*this, true); set_h(*this, (a & 0x0F) == 0x0F); cycles = 4; break; }
        case 0x3E: a = read_imm8(*this, mem); cycles = 8; break;
        case 0x3F: { set_c(*this, !get_c(*this)); set_n(*this, false); set_h(*this, false); cycles = 4; break; }

        case 0x40: b = b; cycles = 4; break;
        case 0x41: b = c; cycles = 4; break;
        case 0x42: b = d; cycles = 4; break;
        case 0x43: b = e; cycles = 4; break;
        case 0x44: b = h; cycles = 4; break;
        case 0x45: b = l; cycles = 4; break;
        case 0x46: b = mem.read(hl); cycles = 8; break;
        case 0x47: b = a; cycles = 4; break;
        case 0x48: c = b; cycles = 4; break;
        case 0x49: c = c; cycles = 4; break;
        case 0x4A: c = d; cycles = 4; break;
        case 0x4B: c = e; cycles = 4; break;
        case 0x4C: c = h; cycles = 4; break;
        case 0x4D: c = l; cycles = 4; break;
        case 0x4E: c = mem.read(hl); cycles = 8; break;
        case 0x4F: c = a; cycles = 4; break;

        case 0x50: d = b; cycles = 4; break;
        case 0x51: d = c; cycles = 4; break;
        case 0x52: d = d; cycles = 4; break;
        case 0x53: d = e; cycles = 4; break;
        case 0x54: d = h; cycles = 4; break;
        case 0x55: d = l; cycles = 4; break;
        case 0x56: d = mem.read(hl); cycles = 8; break;
        case 0x57: d = a; cycles = 4; break;
        case 0x58: e = b; cycles = 4; break;
        case 0x59: e = c; cycles = 4; break;
        case 0x5A: e = d; cycles = 4; break;
        case 0x5B: e = e; cycles = 4; break;
        case 0x5C: e = h; cycles = 4; break;
        case 0x5D: e = l; cycles = 4; break;
        case 0x5E: e = mem.read(hl); cycles = 8; break;
        case 0x5F: e = a; cycles = 4; break;

        case 0x60: h = b; cycles = 4; break;
        case 0x61: h = c; cycles = 4; break;
        case 0x62: h = d; cycles = 4; break;
        case 0x63: h = e; cycles = 4; break;
        case 0x64: h = h; cycles = 4; break;
        case 0x65: h = l; cycles = 4; break;
        case 0x66: h = mem.read(hl); cycles = 8; break;
        case 0x67: h = a; cycles = 4; break;
        case 0x68: l = b; cycles = 4; break;
        case 0x69: l = c; cycles = 4; break;
        case 0x6A: l = d; cycles = 4; break;
        case 0x6B: l = e; cycles = 4; break;
        case 0x6C: l = h; cycles = 4; break;
        case 0x6D: l = l; cycles = 4; break;
        case 0x6E: l = mem.read(hl); cycles = 8; break;
        case 0x6F: l = a; cycles = 4; break;

        case 0x70: mem.write(hl, b); cycles = 8; break;
        case 0x71: mem.write(hl, c); cycles = 8; break;
        case 0x72: mem.write(hl, d); cycles = 8; break;
        case 0x73: mem.write(hl, e); cycles = 8; break;
        case 0x74: mem.write(hl, h); cycles = 8; break;
        case 0x75: mem.write(hl, l); cycles = 8; break;
        case 0x76: halted = true; cycles = 4; break;
        case 0x77: mem.write(hl, a); cycles = 8; break;

        case 0x78: a = b; cycles = 4; break;
        case 0x79: a = c; cycles = 4; break;
        case 0x7A: a = d; cycles = 4; break;
        case 0x7B: a = e; cycles = 4; break;
        case 0x7C: a = h; cycles = 4; break;
        case 0x7D: a = l; cycles = 4; break;
        case 0x7E: a = mem.read(hl); cycles = 8; break;
        case 0x7F: a = a; cycles = 4; break;

        case 0x80: { u16 r = a + b; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (b & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x81: { u16 r = a + c; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (c & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x82: { u16 r = a + d; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (d & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x83: { u16 r = a + e; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (e & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x84: { u16 r = a + h; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (h & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x85: { u16 r = a + l; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (l & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x86: { u8 val = mem.read(hl); u16 r = a + val; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (val & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 8; break; }
        case 0x87: { u16 r = a + a; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (a & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }

        case 0x88: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + b + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (b & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x89: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + c + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (c & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x8A: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + d + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (d & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x8B: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + e + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (e & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x8C: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + h + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (h & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x8D: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + l + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (l & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }
        case 0x8E: { u8 cy = get_c(*this) ? 1 : 0; u8 val = mem.read(hl); u16 r = a + val + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (val & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 8; break; }
        case 0x8F: { u8 cy = get_c(*this) ? 1 : 0; u16 r = a + a + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (a & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 4; break; }

        case 0x90: { u16 r = a - b; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (b & 0xF)); set_c(*this, a < b); a = (u8)r; cycles = 4; break; }
        case 0x91: { u16 r = a - c; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (c & 0xF)); set_c(*this, a < c); a = (u8)r; cycles = 4; break; }
        case 0x92: { u16 r = a - d; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (d & 0xF)); set_c(*this, a < d); a = (u8)r; cycles = 4; break; }
        case 0x93: { u16 r = a - e; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (e & 0xF)); set_c(*this, a < e); a = (u8)r; cycles = 4; break; }
        case 0x94: { u16 r = a - h; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (h & 0xF)); set_c(*this, a < h); a = (u8)r; cycles = 4; break; }
        case 0x95: { u16 r = a - l; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (l & 0xF)); set_c(*this, a < l); a = (u8)r; cycles = 4; break; }
        case 0x96: { u8 val = mem.read(hl); u16 r = a - val; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (val & 0xF)); set_c(*this, a < val); a = (u8)r; cycles = 8; break; }
        case 0x97: { u16 r = a - a; set_z(*this, true); set_n(*this, true); set_h(*this, false); set_c(*this, false); a = 0; cycles = 4; break; }

        case 0x98: { u8 cy = get_c(*this) ? 1 : 0; int r = a - b - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (b & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }
        case 0x99: { u8 cy = get_c(*this) ? 1 : 0; int r = a - c - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (c & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }
        case 0x9A: { u8 cy = get_c(*this) ? 1 : 0; int r = a - d - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (d & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }
        case 0x9B: { u8 cy = get_c(*this) ? 1 : 0; int r = a - e - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (e & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }
        case 0x9C: { u8 cy = get_c(*this) ? 1 : 0; int r = a - h - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (h & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }
        case 0x9D: { u8 cy = get_c(*this) ? 1 : 0; int r = a - l - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (l & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }
        case 0x9E: { u8 cy = get_c(*this) ? 1 : 0; u8 val = mem.read(hl); int r = a - val - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (val & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 8; break; }
        case 0x9F: { u8 cy = get_c(*this) ? 1 : 0; int r = a - a - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, false); set_c(*this, r < 0); a = (u8)r; cycles = 4; break; }

        case 0xA0: { a &= b; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }
        case 0xA1: { a &= c; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }
        case 0xA2: { a &= d; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }
        case 0xA3: { a &= e; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }
        case 0xA4: { a &= h; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }
        case 0xA5: { a &= l; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }
        case 0xA6: { a &= mem.read(hl); set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 8; break; }
        case 0xA7: { a &= a; set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 4; break; }

        case 0xA8: { a ^= b; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xA9: { a ^= c; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xAA: { a ^= d; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xAB: { a ^= e; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xAC: { a ^= h; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xAD: { a ^= l; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xAE: { a ^= mem.read(hl); set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 8; break; }
        case 0xAF: { a ^= a; a = 0; set_z(*this, true); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }

        case 0xB0: { a |= b; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xB1: { a |= c; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xB2: { a |= d; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xB3: { a |= e; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xB4: { a |= h; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xB5: { a |= l; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }
        case 0xB6: { a |= mem.read(hl); set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 8; break; }
        case 0xB7: { a |= a; set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 4; break; }

        case 0xB8: { u16 r = a - b; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (b & 0xF)); set_c(*this, a < b); cycles = 4; break; }
        case 0xB9: { u16 r = a - c; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (c & 0xF)); set_c(*this, a < c); cycles = 4; break; }
        case 0xBA: { u16 r = a - d; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (d & 0xF)); set_c(*this, a < d); cycles = 4; break; }
        case 0xBB: { u16 r = a - e; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (e & 0xF)); set_c(*this, a < e); cycles = 4; break; }
        case 0xBC: { u16 r = a - h; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (h & 0xF)); set_c(*this, a < h); cycles = 4; break; }
        case 0xBD: { u16 r = a - l; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (l & 0xF)); set_c(*this, a < l); cycles = 4; break; }
        case 0xBE: { u8 val = mem.read(hl); u16 r = a - val; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (val & 0xF)); set_c(*this, a < val); cycles = 8; break; }
        case 0xBF: { u16 r = a - a; set_z(*this, true); set_n(*this, true); set_h(*this, false); set_c(*this, false); cycles = 4; break; }

        case 0xC0: { if (!get_z(*this)) { pc = pop(*this, mem); cycles = 20; } else cycles = 8; break; }
        case 0xC1: bc = pop(*this, mem); cycles = 12; break;
        case 0xC2: { u16 addr = read_imm16(*this, mem); if (!get_z(*this)) { pc = addr; cycles = 16; } else cycles = 12; break; }
        case 0xC3: pc = read_imm16(*this, mem); cycles = 16; break;
        case 0xC4: { u16 addr = read_imm16(*this, mem); if (!get_z(*this)) { push(*this, mem, pc); pc = addr; cycles = 24; } else cycles = 12; break; }
        case 0xC5: push(*this, mem, bc); cycles = 16; break;
        case 0xC6: { u8 val = read_imm8(*this, mem); u16 r = a + val; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (val & 0xF) > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 8; break; }
        case 0xC7: push(*this, mem, pc); pc = 0x00; cycles = 16; break;
        case 0xC8: { if (get_z(*this)) { pc = pop(*this, mem); cycles = 20; } else cycles = 8; break; }
        case 0xC9: pc = pop(*this, mem); cycles = 16; break;
        case 0xCA: { u16 addr = read_imm16(*this, mem); if (get_z(*this)) { pc = addr; cycles = 16; } else cycles = 12; break; }
        case 0xCB: cycles = exec_cb(*this, mem); break;
        case 0xCC: { u16 addr = read_imm16(*this, mem); if (get_z(*this)) { push(*this, mem, pc); pc = addr; cycles = 24; } else cycles = 12; break; }
        case 0xCD: { u16 addr = read_imm16(*this, mem); push(*this, mem, pc); pc = addr; cycles = 24; break; }
        case 0xCE: { u8 cy = get_c(*this) ? 1 : 0; u8 val = read_imm8(*this, mem); u16 r = a + val + cy; set_z(*this, (u8)r == 0); set_n(*this, false); set_h(*this, (a & 0xF) + (val & 0xF) + cy > 0xF); set_c(*this, r > 0xFF); a = (u8)r; cycles = 8; break; }
        case 0xCF: push(*this, mem, pc); pc = 0x08; cycles = 16; break;

        case 0xD0: { if (!get_c(*this)) { pc = pop(*this, mem); cycles = 20; } else cycles = 8; break; }
        case 0xD1: de = pop(*this, mem); cycles = 12; break;
        case 0xD2: { u16 addr = read_imm16(*this, mem); if (!get_c(*this)) { pc = addr; cycles = 16; } else cycles = 12; break; }
        case 0xD4: { u16 addr = read_imm16(*this, mem); if (!get_c(*this)) { push(*this, mem, pc); pc = addr; cycles = 24; } else cycles = 12; break; }
        case 0xD5: push(*this, mem, de); cycles = 16; break;
        case 0xD6: { u8 val = read_imm8(*this, mem); u16 r = a - val; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (val & 0xF)); set_c(*this, a < val); a = (u8)r; cycles = 8; break; }
        case 0xD7: push(*this, mem, pc); pc = 0x10; cycles = 16; break;
        case 0xD8: { if (get_c(*this)) { pc = pop(*this, mem); cycles = 20; } else cycles = 8; break; }
        case 0xD9: { pc = pop(*this, mem); ime = true; cycles = 16; break; }
        case 0xDA: { u16 addr = read_imm16(*this, mem); if (get_c(*this)) { pc = addr; cycles = 16; } else cycles = 12; break; }
        case 0xDC: { u16 addr = read_imm16(*this, mem); if (get_c(*this)) { push(*this, mem, pc); pc = addr; cycles = 24; } else cycles = 12; break; }
        case 0xDE: { u8 cy = get_c(*this) ? 1 : 0; u8 val = read_imm8(*this, mem); int r = a - val - cy; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (val & 0xF) + cy); set_c(*this, r < 0); a = (u8)r; cycles = 8; break; }
        case 0xDF: push(*this, mem, pc); pc = 0x18; cycles = 16; break;

        case 0xE0: { u8 offset = read_imm8(*this, mem); mem.write(0xFF00 + offset, a); cycles = 12; break; }
        case 0xE1: hl = pop(*this, mem); cycles = 12; break;
        case 0xE2: mem.write(0xFF00 + c, a); cycles = 8; break;
        case 0xE5: push(*this, mem, hl); cycles = 16; break;
        case 0xE6: { a &= read_imm8(*this, mem); set_z(*this, a == 0); set_n(*this, false); set_h(*this, true); set_c(*this, false); cycles = 8; break; }
        case 0xE7: push(*this, mem, pc); pc = 0x20; cycles = 16; break;
        case 0xE8: { s8 offset = (s8)read_imm8(*this, mem); u32 r = sp + offset; set_z(*this, false); set_n(*this, false); set_h(*this, (sp & 0xF) + (offset & 0xF) > 0xF); set_c(*this, (sp & 0xFF) + (offset & 0xFF) > 0xFF); sp = (u16)r; cycles = 16; break; }
        case 0xE9: pc = hl; cycles = 4; break;
        case 0xEA: { u16 addr = read_imm16(*this, mem); mem.write(addr, a); cycles = 16; break; }
        case 0xEE: { a ^= read_imm8(*this, mem); set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 8; break; }
        case 0xEF: push(*this, mem, pc); pc = 0x28; cycles = 16; break;

        case 0xF0: { u8 offset = read_imm8(*this, mem); a = mem.read(0xFF00 + offset); cycles = 12; break; }
        case 0xF1: af = pop(*this, mem) & 0xFFF0; cycles = 12; break;
        case 0xF2: a = mem.read(0xFF00 + c); cycles = 8; break;
        case 0xF3: ime = false; ime_scheduled = 0; cycles = 4; break;
        case 0xF5: push(*this, mem, af); cycles = 16; break;
        case 0xF6: { a |= read_imm8(*this, mem); set_z(*this, a == 0); set_n(*this, false); set_h(*this, false); set_c(*this, false); cycles = 8; break; }
        case 0xF7: push(*this, mem, pc); pc = 0x30; cycles = 16; break;
        case 0xF8: { s8 offset = (s8)read_imm8(*this, mem); u32 r = sp + offset; set_z(*this, false); set_n(*this, false); set_h(*this, (sp & 0xF) + (offset & 0xF) > 0xF); set_c(*this, (sp & 0xFF) + (offset & 0xFF) > 0xFF); hl = (u16)r; cycles = 12; break; }
        case 0xF9: sp = hl; cycles = 8; break;
        case 0xFA: { u16 addr = read_imm16(*this, mem); a = mem.read(addr); cycles = 16; break; }
        case 0xFB: ime_scheduled = 1; cycles = 4; break;
        case 0xFE: { u8 val = read_imm8(*this, mem); u16 r = a - val; set_z(*this, (u8)r == 0); set_n(*this, true); set_h(*this, (a & 0xF) < (val & 0xF)); set_c(*this, a < val); cycles = 8; break; }
        case 0xFF: push(*this, mem, pc); pc = 0x38; cycles = 16; break;

        default: cycles = 4; break;
    }

    return cycles;
}
