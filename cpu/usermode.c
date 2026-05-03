#include "usermode.h"
#include "../drivers/screen.h"

static int usermode_initialized = 0;
static int usermode_ring3_available = 0;
static int usermode_current_ring = USERMODE_RING_KERNEL;

void init_usermode() {
    /*
     * User/kernel separation foundation.
     *
     * This update is intentionally safe:
     * - The kernel still runs in ring 0.
     * - Ring 3 is NOT entered yet.
     * - No TSS stack switch is active yet.
     * - No user program loader is active yet.
     *
     * This gives the shell a stable status layer before we touch GDT/TSS.
     */
    usermode_initialized = 1;
    usermode_ring3_available = 0;
    usermode_current_ring = USERMODE_RING_KERNEL;
}

int usermode_is_initialized() {
    return usermode_initialized;
}

int usermode_is_ring3_available() {
    return usermode_ring3_available;
}

int usermode_get_current_ring() {
    return usermode_current_ring;
}

void usermode_print_help() {
    kprint("User/kernel separation commands:\n");
    kprint("USERMODE          - Show usermode foundation status\n");
    kprint("RING3             - Alias for USERMODE\n");
    kprint("\n");
    kprint("Current stage:\n");
    kprint("Foundation only. Kernel runs in ring 0.\n");
    kprint("Ring 3 switch will be added later with GDT/TSS support.\n");
}

void usermode_print_status() {
    kprint("User/kernel separation status:\n");

    kprint("Initialized: ");
    if (usermode_initialized) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }

    kprint("Current CPU privilege ring: ");
    if (usermode_current_ring == USERMODE_RING_KERNEL) {
        kprint("ring 0 kernel mode\n");
    } else if (usermode_current_ring == USERMODE_RING_USER) {
        kprint("ring 3 user mode\n");
    } else {
        kprint("unknown\n");
    }

    kprint("Ring 3 available: ");
    if (usermode_ring3_available) {
        kprint("yes\n");
    } else {
        kprint("not yet\n");
    }

    kprint("TSS active: not yet\n");
    kprint("User stack active: not yet\n");
    kprint("User program loader: not yet\n");
    kprint("Syscall path: int 0x80 available for future user programs\n");

    kprint("\nNext stages:\n");
    kprint("1. Add GDT user code/data descriptors\n");
    kprint("2. Add TSS and kernel stack pointer\n");
    kprint("3. Add ring 3 test jump\n");
    kprint("4. Add user program loader\n");
}
