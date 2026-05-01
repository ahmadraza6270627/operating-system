#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void init_timer(u32 freq);

u32 get_timer_ticks();
u32 get_timer_seconds();

void timer_wait(u32 ticks);

#endif