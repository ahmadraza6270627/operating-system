#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

#define SYSCALL_INTERRUPT 0x80

#define SYS_PRINT       1
#define SYS_CLEAR       2
#define SYS_GET_TICKS   3
#define SYS_GET_VERSION 4

void init_syscalls();

u32 syscall_dispatch(u32 syscall_number, u32 arg1, u32 arg2, u32 arg3);

u32 syscall_call0(u32 syscall_number);
u32 syscall_call1(u32 syscall_number, u32 arg1);
u32 syscall_call2(u32 syscall_number, u32 arg1, u32 arg2);
u32 syscall_call3(u32 syscall_number, u32 arg1, u32 arg2, u32 arg3);

int syscall_is_enabled();
int syscall_get_count();
char *syscall_get_name(u32 syscall_number);
char *syscall_get_description(u32 syscall_number);
char *syscall_get_mode();

#endif