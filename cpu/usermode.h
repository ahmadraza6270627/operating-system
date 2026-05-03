#ifndef USERMODE_H
#define USERMODE_H

#include "types.h"

#define USERMODE_RING_KERNEL 0
#define USERMODE_RING_USER   3

void init_usermode();

int usermode_is_initialized();
int usermode_is_ring3_available();
int usermode_get_current_ring();

void usermode_print_status();
void usermode_print_help();

#endif
