#include "gb_apu.h"
#include <cstring>

static const u8 duty_waves[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0},
};

static const u16 noise_divisors[8] = {
    8, 16, 32, 48, 64, 80, 96, 112
};

void APU::reset() {
    buffer_pos = 0;
    frame_seq_cycle = 0;
    frame_seq_step = 0;
    sample_counter = 0;
    std::memset(&ch1, 0, sizeof(ch1));
    std::memset(&ch2, 0, sizeof(ch2));
    std::memset(&ch3, 0, sizeof(ch3));
    std::memset(&ch4, 0, sizeof(ch4));
    ch4.lfsr = 0x7FFF;
}

u16 APU::noise_period() const {
    u16 base = noise_divisors[ch4.divisor_code & 7];
    return base << ch4.shift_amount;
}

void APU::write_reg(u16 addr, u8 val) {
    switch (addr) {
        case NR10:
            ch1.sweep_period = (val >> 4) & 7;
            ch1.sweep_dir    = (val >> 3) & 1;
            ch1.sweep_shift  = val & 7;
            break;
        case NR11:
            ch1.duty = (val >> 6) & 3;
            ch1.length_counter = 64 - (val & 63);
            break;
        case NR12:
            ch1.volume          = (val >> 4) & 0xF;
            ch1.envelope_dir    = (val >> 3) & 1;
            ch1.envelope_period = val & 7;
            ch1.dac_enabled     = (val & 0xF8) != 0;
            if (!ch1.dac_enabled) ch1.enabled = false;
            break;
        case NR13:
            ch1.frequency = (ch1.frequency & 0x700) | val;
            break;
        case NR14:
            ch1.frequency = (ch1.frequency & 0xFF) | ((val & 7) << 8);
            ch1.length_enable = (val >> 6) & 1;
            break;

        case NR21:
            ch2.duty = (val >> 6) & 3;
            ch2.length_counter = 64 - (val & 63);
            break;
        case NR22:
            ch2.volume          = (val >> 4) & 0xF;
            ch2.envelope_dir    = (val >> 3) & 1;
            ch2.envelope_period = val & 7;
            ch2.dac_enabled     = (val & 0xF8) != 0;
            if (!ch2.dac_enabled) ch2.enabled = false;
            break;
        case NR23:
            ch2.frequency = (ch2.frequency & 0x700) | val;
            break;
        case NR24:
            ch2.frequency = (ch2.frequency & 0xFF) | ((val & 7) << 8);
            ch2.length_enable = (val >> 6) & 1;
            break;

        case NR30:
            ch3.dac_enabled = (val & 0x80) != 0;
            if (!ch3.dac_enabled) ch3.enabled = false;
            break;
        case NR31:
            ch3.length_counter = 256 - val;
            break;
        case NR32:
            ch3.volume_code = (val >> 5) & 3;
            break;
        case NR33:
            ch3.frequency = (ch3.frequency & 0x700) | val;
            break;
        case NR34:
            ch3.frequency = (ch3.frequency & 0xFF) | ((val & 7) << 8);
            ch3.length_enable = (val >> 6) & 1;
            break;

        case NR41:
            ch4.length_counter = 64 - (val & 63);
            break;
        case NR42:
            ch4.volume          = (val >> 4) & 0xF;
            ch4.envelope_dir    = (val >> 3) & 1;
            ch4.envelope_period = val & 7;
            ch4.dac_enabled     = (val & 0xF8) != 0;
            if (!ch4.dac_enabled) ch4.enabled = false;
            break;
        case NR43:
            ch4.divisor_code  = val & 7;
            ch4.width_mode    = (val >> 3) & 1;
            ch4.shift_amount  = (val >> 4) & 0xF;
            break;
        case NR44:
            ch4.length_enable = (val >> 6) & 1;
            break;

        default:
            break;
    }

    if (addr == NR14 && (val & 0x80)) trigger_ch1_helper(val);
    if (addr == NR24 && (val & 0x80)) trigger_ch2_helper(val);
    if (addr == NR34 && (val & 0x80)) trigger_ch3_helper(val);
    if (addr == NR44 && (val & 0x80)) trigger_ch4_helper(val);
}

void APU::trigger_ch1_helper(u8 nr14_val) {
    ch1.enabled = ch1.dac_enabled;
    ch1.freq_counter = (2048 - ch1.frequency) * 4;
    ch1.duty_pos = 0;
    ch1.envelope_counter = ch1.envelope_period;
    if (ch1.length_counter == 0) ch1.length_counter = 64;
    ch1.shadow_freq = ch1.frequency;
    ch1.sweep_counter = ch1.sweep_period ? ch1.sweep_period : 8;
    ch1.sweep_enabled = ch1.sweep_period > 0 || ch1.sweep_shift > 0;
    if (ch1.sweep_shift > 0) {
        u16 delta = ch1.shadow_freq >> ch1.sweep_shift;
        u16 new_freq = ch1.sweep_dir
            ? ch1.shadow_freq - delta
            : ch1.shadow_freq + delta;
        if (new_freq > 2047) ch1.enabled = false;
    }
}

void APU::trigger_ch2_helper(u8 nr24_val) {
    ch2.enabled = ch2.dac_enabled;
    ch2.freq_counter = (2048 - ch2.frequency) * 4;
    ch2.duty_pos = 0;
    ch2.envelope_counter = ch2.envelope_period;
    if (ch2.length_counter == 0) ch2.length_counter = 64;
}

void APU::trigger_ch3_helper(u8 nr34_val) {
    ch3.enabled = ch3.dac_enabled;
    ch3.freq_counter = (2048 - ch3.frequency) * 2;
    ch3.sample_pos = 0;
    if (ch3.length_counter == 0) ch3.length_counter = 256;
}

void APU::trigger_ch4_helper(u8 nr44_val) {
    ch4.enabled = ch4.dac_enabled;
    ch4.freq_counter = noise_period();
    ch4.lfsr = 0x7FFF;
    ch4.envelope_counter = ch4.envelope_period;
    if (ch4.length_counter == 0) ch4.length_counter = 64;
}

void APU::tick_square(SquareChannel& ch) {
    if (!ch.enabled || !ch.dac_enabled) return;
    ch.freq_counter--;
    if (ch.freq_counter <= 0) {
        ch.freq_counter = (2048 - ch.frequency) * 4;
        ch.duty_pos = (ch.duty_pos + 1) & 7;
    }
}

void APU::tick_wave(WaveChannel& ch) {
    if (!ch.enabled || !ch.dac_enabled) return;
    ch.freq_counter--;
    if (ch.freq_counter <= 0) {
        ch.freq_counter = (2048 - ch.frequency) * 2;
        ch.sample_pos = (ch.sample_pos + 1) & 31;
    }
}

void APU::tick_noise(NoiseChannel& ch) {
    if (!ch.enabled || !ch.dac_enabled) return;
    ch.freq_counter--;
    if (ch.freq_counter <= 0) {
        ch.freq_counter = noise_divisors[ch.divisor_code & 7] << ch.shift_amount;
        u8 xor_r = ((ch.lfsr >> 0) & 1) ^ ((ch.lfsr >> 1) & 1);
        ch.lfsr = (ch.lfsr >> 1) | (xor_r << 14);
        if (ch.width_mode)
            ch.lfsr = (ch.lfsr & ~(1 << 6)) | (xor_r << 6);
    }
}

float APU::read_square(const SquareChannel& ch) {
    if (!ch.enabled || !ch.dac_enabled) return 0.0f;
    u8 wave_out = duty_waves[ch.duty][ch.duty_pos];
    float dac_in = wave_out ? (float)ch.volume : 0.0f;
    return (dac_in / 7.5f) - 1.0f;
}

float APU::read_wave(const WaveChannel& ch, Memory& mem) {
    if (!ch.enabled || !ch.dac_enabled) return 0.0f;
    u16 addr = 0xFF30 + ch.sample_pos / 2;
    u8 byte = mem.read(addr);
    u8 nibble = (ch.sample_pos & 1) ? (byte & 0x0F) : (byte >> 4);
    u8 shifted;
    switch (ch.volume_code) {
        case 0: shifted = 0; break;
        case 1: shifted = nibble; break;
        case 2: shifted = nibble >> 1; break;
        case 3: shifted = nibble >> 2; break;
        default: shifted = 0; break;
    }
    return (shifted / 7.5f) - 1.0f;
}

float APU::read_noise(const NoiseChannel& ch) {
    if (!ch.enabled || !ch.dac_enabled) return 0.0f;
    float dac_in = (ch.lfsr & 1) == 0 ? (float)ch.volume : 0.0f;
    return (dac_in / 7.5f) - 1.0f;
}

void APU::tick_frame_sequencer() {
    frame_seq_step = (frame_seq_step + 1) & 7;

    if ((frame_seq_step & 1) == 0) {
        auto tick_length = [](auto& ch, int max_len) {
            if (ch.length_enable && ch.length_counter > 0) {
                ch.length_counter--;
                if (ch.length_counter == 0) ch.enabled = false;
            }
        };
        tick_length(ch1, 64);
        tick_length(ch2, 64);
        tick_length(ch3, 256);
        tick_length(ch4, 64);
    }

    if (frame_seq_step == 2 || frame_seq_step == 6) {
        if (ch1.sweep_enabled && ch1.sweep_period > 0) {
            ch1.sweep_counter--;
            if (ch1.sweep_counter <= 0) {
                ch1.sweep_counter = ch1.sweep_period ? ch1.sweep_period : 8;
                u16 delta = ch1.shadow_freq >> ch1.sweep_shift;
                u16 new_freq;
                if (ch1.sweep_dir)
                    new_freq = ch1.shadow_freq - delta;
                else
                    new_freq = ch1.shadow_freq + delta;

                if (new_freq > 2047) {
                    ch1.enabled = false;
                } else if (ch1.sweep_shift > 0) {
                    ch1.frequency = new_freq;
                    ch1.shadow_freq = new_freq;
                    u16 delta2 = new_freq >> ch1.sweep_shift;
                    u16 check = ch1.sweep_dir
                        ? new_freq - delta2
                        : new_freq + delta2;
                    if (check > 2047) ch1.enabled = false;
                }
            }
        }
    }

    if (frame_seq_step == 7) {
        auto tick_env = [](auto& ch) {
            if (ch.envelope_period > 0 && ch.dac_enabled) {
                ch.envelope_counter--;
                if (ch.envelope_counter <= 0) {
                    ch.envelope_counter = ch.envelope_period;
                    if (ch.envelope_dir) {
                        if (ch.volume < 15) ch.volume++;
                    } else {
                        if (ch.volume > 0) ch.volume--;
                    }
                }
            }
        };
        tick_env(ch1);
        tick_env(ch2);
        tick_env(ch4);
    }
}

void APU::update_nr52(Memory& mem) {
    u8 nr52 = mem.read(NR52);
    if (nr52 & 0x80) {
        u8 status = 0x80;
        if (ch1.enabled && ch1.dac_enabled) status |= 1;
        if (ch2.enabled && ch2.dac_enabled) status |= 2;
        if (ch3.enabled && ch3.dac_enabled) status |= 4;
        if (ch4.enabled && ch4.dac_enabled) status |= 8;
        mem.io_regs[NR52 - 0xFF00] = status;
    }
}

void APU::generate_sample(Memory& mem) {
    if (buffer_pos >= (int)audio_buffer.size()) return;

    u8 nr50 = mem.io_regs[NR50 - 0xFF00];
    u8 nr51 = mem.io_regs[NR51 - 0xFF00];

    float ch1_out = read_square(ch1);
    float ch2_out = read_square(ch2);
    float ch3_out = read_wave(ch3, mem);
    float ch4_out = read_noise(ch4);

    float left = 0.0f;
    float right = 0.0f;

    if (nr51 & 0x10) left  += ch1_out;
    if (nr51 & 0x01) right += ch1_out;
    if (nr51 & 0x20) left  += ch2_out;
    if (nr51 & 0x02) right += ch2_out;
    if (nr51 & 0x40) left  += ch3_out;
    if (nr51 & 0x04) right += ch3_out;
    if (nr51 & 0x80) left  += ch4_out;
    if (nr51 & 0x08) right += ch4_out;

    int left_vol  = ((nr50 >> 4) & 7) + 1;
    int right_vol = (nr50 & 7) + 1;
    left  *= left_vol / 8.0f;
    right *= right_vol / 8.0f;

    left  *= 0.25f;
    right *= 0.25f;

    float mono = (left + right) * 0.5f;
    if (mono > 1.0f) mono = 1.0f;
    if (mono < -1.0f) mono = -1.0f;

    audio_buffer[buffer_pos++] = (s16)(mono * 32767);
}

void APU::step(u8 cycles, Memory& mem) {
    u8 nr52 = mem.io_regs[NR52 - 0xFF00];
    if (!(nr52 & 0x80)) return;

    for (int i = 0; i < cycles; i++) {
        tick_square(ch1);
        tick_square(ch2);
        tick_wave(ch3);
        tick_noise(ch4);

        frame_seq_cycle++;
        if (frame_seq_cycle >= 8192) {
            frame_seq_cycle -= 8192;
            tick_frame_sequencer();
        }

        sample_counter += SAMPLE_RATE;
        if (sample_counter >= CPU_CLOCK) {
            sample_counter -= CPU_CLOCK;
            generate_sample(mem);
        }
    }

    update_nr52(mem);
}