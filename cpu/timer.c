#include "timer.h"
#include "isr.h"
#include "ports.h"

static u32 timer_ticks = 0;
static u32 timer_frequency = 50;

static void timer_callback(registers_t *regs) {
    regs = regs;

    timer_ticks++;
}

void init_timer(u32 freq) {
    u32 divisor;
    u8 low;
    u8 high;

    if (freq == 0) {
        freq = 50;
    }

    timer_frequency = freq;

    add_interrupt_handler(IRQ0, timer_callback);

    divisor = 1193180 / freq;

    low = (u8)(divisor & 0xFF);
    high = (u8)((divisor >> 8) & 0xFF);

    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
}

u32 get_timer_ticks() {
    return timer_ticks;
}

u32 get_timer_seconds() {
    if (timer_frequency == 0) {
        return 0;
    }

    return timer_ticks / timer_frequency;
}

void timer_wait(u32 ticks) {
    u32 target;

    target = timer_ticks + ticks;

    while (timer_ticks < target) {
        asm volatile("hlt");
    }
}