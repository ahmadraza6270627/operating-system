#include "programs.h"
#include "../drivers/screen.h"
#include "../cpu/timer.h"
#include "../libc/string.h"
#include "../libc/heap.h"
#include "../libc/ramfs.h"

typedef void (*program_entry_t)(char *args);

typedef struct program {
    char *name;
    char *description;
    program_entry_t entry;
} program_t;

static int programs_ready = 0;

static void print_uint_local(u32 value) {
    char str[32];
    int i = 0;
    int j;
    char temp;

    if (value == 0) {
        kprint("0");
        return;
    }

    while (value > 0) {
        str[i] = (value % 10) + '0';
        value = value / 10;
        i++;
    }

    str[i] = '\0';

    for (j = 0; j < i / 2; j++) {
        temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    kprint(str);
}

static char hex_digit_local(u32 value) {
    value = value & 0xF;

    if (value < 10) {
        return '0' + value;
    }

    return 'A' + (value - 10);
}

static void print_hex32_local(u32 value) {
    char str[11];
    int i;

    str[0] = '0';
    str[1] = 'x';

    for (i = 0; i < 8; i++) {
        str[2 + i] = hex_digit_local(value >> ((7 - i) * 4));
    }

    str[10] = '\0';

    kprint(str);
}

static void program_hello(char *args) {
    kprint("Hello from internal program HELLO.\n");

    if (args != 0 && args[0] != '\0') {
        kprint("Arguments: ");
        kprint(args);
        kprint("\n");
    }
}

static void program_clock(char *args) {
    args = args;

    kprint("CLOCK program\n");
    kprint("Ticks: ");
    print_uint_local(get_timer_ticks());
    kprint("\n");
    kprint("Uptime seconds: ");
    print_uint_local(get_timer_seconds());
    kprint("\n");
}

static void program_mem(char *args) {
    args = args;

    kprint("MEM program\n");

    kprint("Heap start: ");
    print_hex32_local(heap_get_start());
    kprint("\n");

    kprint("Heap end: ");
    print_hex32_local(heap_get_end());
    kprint("\n");

    kprint("Heap total: ");
    print_uint_local(heap_get_total());
    kprint(" bytes\n");

    kprint("Heap used: ");
    print_uint_local(heap_get_used());
    kprint(" bytes\n");

    kprint("Heap free: ");
    print_uint_local(heap_get_free());
    kprint(" bytes\n");
}

static void program_files(char *args) {
    args = args;

    kprint("FILES program\n");
    kprint("RAM files used: ");
    print_uint_local((u32)ramfs_file_count());
    kprint("\n");
    kprint("Use LS, CAT, WRITE, APPEND, RM from shell for file operations.\n");
}

static void program_colors(char *args) {
    args = args;

    kprint_colored("WHITE\n", WHITE_ON_BLACK);
    kprint_colored("GREEN\n", LIGHT_GREEN_ON_BLACK);
    kprint_colored("CYAN\n", LIGHT_CYAN_ON_BLACK);
    kprint_colored("RED\n", LIGHT_RED_ON_BLACK);
    kprint_colored("YELLOW\n", YELLOW_ON_BLACK);
    kprint_colored("BLUE\n", LIGHT_BLUE_ON_BLACK);
    kprint_colored("MAGENTA\n", LIGHT_MAGENTA_ON_BLACK);
    kprint_colored("GREY\n", LIGHT_GREY_ON_BLACK);
}

static void program_about(char *args) {
    args = args;

    kprint("ABOUT program\n");
    kprint("Simple_OS / MyOS internal program foundation v1.\n");
    kprint("These are kernel-managed programs, not real user-mode apps yet.\n");
}

static program_t program_table[] = {
    {"HELLO",  "Print hello message and arguments", program_hello},
    {"CLOCK",  "Show timer ticks and uptime",       program_clock},
    {"MEM",    "Show heap memory information",      program_mem},
    {"FILES",  "Show RAM filesystem summary",       program_files},
    {"COLORS", "Show VGA text colors",              program_colors},
    {"ABOUT",  "Show program system information",   program_about}
};

#define PROGRAM_COUNT (sizeof(program_table) / sizeof(program_table[0]))

void init_programs() {
    programs_ready = 1;
}

void program_help() {
    kprint("Program manager commands:\n");
    kprint("PROGRAMS              - List internal programs\n");
    kprint("PROGS                 - Alias for PROGRAMS\n");
    kprint("PROGRAM HELP          - Show this help\n");
    kprint("PROGRAM INFO NAME     - Show program information\n");
    kprint("RUNPROG NAME          - Run internal program\n");
    kprint("RUNPROG NAME ARGS     - Run internal program with arguments\n");
    kprint("EXEC NAME             - Alias for RUNPROG\n");
}

void program_list() {
    unsigned int i;

    if (!programs_ready) {
        kprint("Program manager is not initialized.\n");
        return;
    }

    kprint("Internal programs:\n");

    for (i = 0; i < PROGRAM_COUNT; i++) {
        kprint(program_table[i].name);
        kprint(" - ");
        kprint(program_table[i].description);
        kprint("\n");
    }
}

void program_info(char *name) {
    unsigned int i;

    if (name == 0 || name[0] == '\0') {
        kprint("Usage: PROGRAM INFO NAME\n");
        return;
    }

    for (i = 0; i < PROGRAM_COUNT; i++) {
        if (strcmp_ignore_case(name, program_table[i].name) == 0) {
            kprint("Program name: ");
            kprint(program_table[i].name);
            kprint("\n");

            kprint("Description: ");
            kprint(program_table[i].description);
            kprint("\n");

            kprint("Type: internal kernel-managed program\n");
            return;
        }
    }

    kprint("Program not found.\n");
}

int program_run(char *name, char *args) {
    unsigned int i;

    if (!programs_ready) {
        kprint("Program manager is not initialized.\n");
        return 0;
    }

    if (name == 0 || name[0] == '\0') {
        kprint("Usage: RUNPROG NAME\n");
        return 0;
    }

    for (i = 0; i < PROGRAM_COUNT; i++) {
        if (strcmp_ignore_case(name, program_table[i].name) == 0) {
            kprint("Running program: ");
            kprint(program_table[i].name);
            kprint("\n");

            program_table[i].entry(args);

            kprint("Program finished.\n");
            return 1;
        }
    }

    kprint("Program not found. Use PROGRAMS.\n");
    return 0;
}