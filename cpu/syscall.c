#include "syscall.h"
#include "idt.h"
#include "timer.h"
#include "../drivers/screen.h"
#include "../libc/string.h"

extern void isr128();

static int syscall_enabled = 0;

static char syscall_version[] = "Simple_OS syscall ABI v2 real int 0x80";
static char syscall_mode[] = "real int 0x80 kernel-mode test path";

void init_syscalls() {
    /*
     * Real int 0x80 retry:
     * IDT vector 0x80 points to isr128 in cpu/interrupt.asm.
     * The assembly stub reads:
     * eax = syscall number
     * ebx = arg1
     * ecx = arg2
     * edx = arg3
     * Then it calls syscall_dispatch().
     */
    set_idt_gate(SYSCALL_INTERRUPT, (u32)isr128);
    syscall_enabled = 1;
}

int syscall_is_enabled() {
    return syscall_enabled;
}

int syscall_get_count() {
    return 4;
}

char *syscall_get_mode() {
    return syscall_mode;
}

char *syscall_get_name(u32 syscall_number) {
    if (syscall_number == SYS_PRINT) {
        return "SYS_PRINT";
    }

    if (syscall_number == SYS_CLEAR) {
        return "SYS_CLEAR";
    }

    if (syscall_number == SYS_GET_TICKS) {
        return "SYS_GET_TICKS";
    }

    if (syscall_number == SYS_GET_VERSION) {
        return "SYS_GET_VERSION";
    }

    return "UNKNOWN";
}

char *syscall_get_description(u32 syscall_number) {
    if (syscall_number == SYS_PRINT) {
        return "Print string to screen";
    }

    if (syscall_number == SYS_CLEAR) {
        return "Clear VGA text screen";
    }

    if (syscall_number == SYS_GET_TICKS) {
        return "Return PIT timer ticks";
    }

    if (syscall_number == SYS_GET_VERSION) {
        return "Return syscall ABI string address";
    }

    return "Unknown syscall";
}

u32 syscall_dispatch(u32 syscall_number, u32 arg1, u32 arg2, u32 arg3) {
    char *text;

    arg2 = arg2;
    arg3 = arg3;

    if (syscall_number == SYS_PRINT) {
        text = (char *)arg1;

        if (text != 0) {
            kprint(text);
            return strlen(text);
        }

        return 0;
    }

    if (syscall_number == SYS_CLEAR) {
        clear_screen();
        return 1;
    }

    if (syscall_number == SYS_GET_TICKS) {
        return get_timer_ticks();
    }

    if (syscall_number == SYS_GET_VERSION) {
        return (u32)syscall_version;
    }

    return 0xFFFFFFFF;
}

u32 syscall_call0(u32 syscall_number) {
    u32 result;

    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(syscall_number)
        : "memory"
    );

    return result;
}

u32 syscall_call1(u32 syscall_number, u32 arg1) {
    u32 result;

    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(syscall_number), "b"(arg1)
        : "memory"
    );

    return result;
}

u32 syscall_call2(u32 syscall_number, u32 arg1, u32 arg2) {
    u32 result;

    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(syscall_number), "b"(arg1), "c"(arg2)
        : "memory"
    );

    return result;
}

u32 syscall_call3(u32 syscall_number, u32 arg1, u32 arg2, u32 arg3) {
    u32 result;

    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(syscall_number), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );

    return result;
}
