#include "gb_timer.h"
#include "gb_memory.h"

void Timer::reset() {
    div_counter = 0;
    tima_counter = 0;
    prev_div_bit = 0;
}

void Timer::step(u8 cycles, Memory& mem) {
    for (int i = 0; i < cycles; i++) {
        div_counter++;
        if (div_counter >= 256) {
            div_counter = 0;
            u8 div = mem.read(DIV);
            mem.write(DIV, (u8)(div + 1));
        }

        u8 tac = mem.read(TAC);
        if (tac & 0x04) {
            u8 clock_select = tac & 0x03;
            u32 div_bit = 0;
            switch (clock_select) {
                case 0: div_bit = (div_counter & (1 << 9)) ? 1 : 0; break;
                case 1: div_bit = (div_counter & (1 << 3)) ? 1 : 0; break;
                case 2: div_bit = (div_counter & (1 << 5)) ? 1 : 0; break;
                case 3: div_bit = (div_counter & (1 << 7)) ? 1 : 0; break;
            }

            if (prev_div_bit == 1 && div_bit == 0) {
                u8 tima = mem.read(TIMA);
                tima++;
                if (tima == 0) {
                    tima = mem.read(TMA);
                    u8 iflag = mem.read(IF);
                    iflag |= INT_TIMER;
                    mem.write(IF, iflag);
                }
                mem.write(TIMA, tima);
            }
            prev_div_bit = div_bit;
        }
    }
}
