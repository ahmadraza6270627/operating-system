#include "process.h"
#include "../drivers/screen.h"
#include "../cpu/timer.h"

typedef struct process_entry {
    int used;
    int pid;
    int status;
    char name[32];
    char args[128];
    u32 start_ticks;
    u32 end_ticks;
} process_entry_t;

static process_entry_t process_table[PROCESS_MAX];

static int process_ready = 0;
static int process_next_pid = 1;
static int process_next_slot = 0;
static int process_current_pid = 0;

static void copy_string_limited_local(char *dest, char *src, int max_len) {
    int i = 0;

    if (max_len <= 0) {
        return;
    }

    if (src == 0) {
        dest[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

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

static char *status_text(int status) {
    if (status == PROCESS_STATUS_RUNNING) {
        return "RUNNING";
    }

    if (status == PROCESS_STATUS_SUCCESS) {
        return "SUCCESS";
    }

    if (status == PROCESS_STATUS_FAILED) {
        return "FAILED";
    }

    return "EMPTY";
}

static process_entry_t *find_process_by_pid(int pid) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].used && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }

    return 0;
}

void init_processes() {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        process_table[i].used = 0;
        process_table[i].pid = 0;
        process_table[i].status = PROCESS_STATUS_EMPTY;
        process_table[i].name[0] = '\0';
        process_table[i].args[0] = '\0';
        process_table[i].start_ticks = 0;
        process_table[i].end_ticks = 0;
    }

    process_ready = 1;
    process_next_pid = 1;
    process_next_slot = 0;
    process_current_pid = 0;
}

int process_start(char *name, char *args) {
    process_entry_t *p;

    if (!process_ready) {
        return 0;
    }

    p = &process_table[process_next_slot];

    process_next_slot++;

    if (process_next_slot >= PROCESS_MAX) {
        process_next_slot = 0;
    }

    p->used = 1;
    p->pid = process_next_pid;
    p->status = PROCESS_STATUS_RUNNING;
    p->start_ticks = get_timer_ticks();
    p->end_ticks = 0;

    copy_string_limited_local(p->name, name, 32);
    copy_string_limited_local(p->args, args, 128);

    process_current_pid = p->pid;

    process_next_pid++;

    if (process_next_pid < 1) {
        process_next_pid = 1;
    }

    return p->pid;
}

void process_finish(int pid, int result) {
    process_entry_t *p;

    if (!process_ready) {
        return;
    }

    p = find_process_by_pid(pid);

    if (p == 0) {
        return;
    }

    if (result) {
        p->status = PROCESS_STATUS_SUCCESS;
    } else {
        p->status = PROCESS_STATUS_FAILED;
    }

    p->end_ticks = get_timer_ticks();

    if (process_current_pid == pid) {
        process_current_pid = 0;
    }
}

int process_count() {
    int i;
    int count = 0;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].used) {
            count++;
        }
    }

    return count;
}

int process_get_current_pid() {
    return process_current_pid;
}

void process_help() {
    kprint("Process manager commands:\n");
    kprint("PS                    - List process history\n");
    kprint("PROCESS HELP          - Show this help\n");
    kprint("PROCESS LIST          - List process history\n");
    kprint("PROCESS STATUS        - Show process manager status\n");
    kprint("PROCESS COUNT         - Show stored process count\n");
    kprint("PROCESS CLEAR         - Clear process history\n");
    kprint("PROC ...              - Alias for PROCESS\n");
    kprint("\nNote: processes are simulated records for internal programs.\n");
    kprint("They are not real multitasking processes yet.\n");
}

void process_status() {
    kprint("Process manager status:\n");

    kprint("Initialized: ");
    if (process_ready) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }

    kprint("Max records: ");
    print_uint_local((u32)PROCESS_MAX);
    kprint("\n");

    kprint("Stored records: ");
    print_uint_local((u32)process_count());
    kprint("\n");

    kprint("Next PID: ");
    print_uint_local((u32)process_next_pid);
    kprint("\n");

    kprint("Current PID: ");
    print_uint_local((u32)process_current_pid);
    kprint("\n");

    kprint("Mode: internal process history, no multitasking yet\n");
}

void process_list() {
    int i;
    u32 duration;

    if (!process_ready) {
        kprint("Process manager is not initialized.\n");
        return;
    }

    if (process_count() == 0) {
        kprint("No process records.\n");
        return;
    }

    kprint("Process history:\n");

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].used) {
            kprint("PID ");
            print_uint_local((u32)process_table[i].pid);
            kprint(" | ");

            kprint(status_text(process_table[i].status));
            kprint(" | ");

            kprint(process_table[i].name);
            kprint(" | start ");
            print_uint_local(process_table[i].start_ticks);

            kprint(" end ");
            if (process_table[i].end_ticks == 0) {
                kprint("-");
                duration = 0;
            } else {
                print_uint_local(process_table[i].end_ticks);

                if (process_table[i].end_ticks >= process_table[i].start_ticks) {
                    duration = process_table[i].end_ticks - process_table[i].start_ticks;
                } else {
                    duration = 0;
                }
            }

            kprint(" duration ");
            print_uint_local(duration);

            if (process_table[i].args[0] != '\0') {
                kprint(" | args: ");
                kprint(process_table[i].args);
            }

            kprint("\n");
        }
    }
}

void process_clear() {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        process_table[i].used = 0;
        process_table[i].pid = 0;
        process_table[i].status = PROCESS_STATUS_EMPTY;
        process_table[i].name[0] = '\0';
        process_table[i].args[0] = '\0';
        process_table[i].start_ticks = 0;
        process_table[i].end_ticks = 0;
    }

    process_current_pid = 0;
    process_next_slot = 0;

    kprint("Process history cleared.\n");
}
