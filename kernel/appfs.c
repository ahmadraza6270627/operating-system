#include "appfs.h"
#include "programs.h"
#include "process.h"
#include "../drivers/screen.h"
#include "../cpu/timer.h"
#include "../cpu/syscall.h"
#include "../cpu/types.h"
#include "../libc/string.h"
#include "../libc/ramfs.h"

#define APPFS_SCRIPT_SIZE RAMFS_CONTENT_SIZE
#define APPFS_COMMAND_SIZE 256
#define APPFS_MAX_DEPTH 2

static int appfs_ready = 0;
static int appfs_total_runs = 0;
static int appfs_last_command_count = 0;
static int appfs_last_result = 0;
static char appfs_last_name[RAMFS_NAME_SIZE];

static char *skip_spaces_local(char *s) {
    while (*s != '\0' && is_space(*s)) {
        s++;
    }

    return s;
}

static void trim_right_local(char *s) {
    int len = strlen(s);

    while (len > 0 && is_space(s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static char *read_word_local(char *input, char word[], int max_len) {
    int i = 0;

    input = skip_spaces_local(input);

    while (*input != '\0' && !is_space(*input)) {
        if (i < max_len - 1) {
            word[i] = *input;
            i++;
        }

        input++;
    }

    word[i] = '\0';
    return input;
}

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

void init_appfs() {
    appfs_ready = 1;
    appfs_total_runs = 0;
    appfs_last_command_count = 0;
    appfs_last_result = 0;
    appfs_last_name[0] = '\0';
}

void appfs_help() {
    kprint("External RAM-file app commands:\n");
    kprint("APPSFS HELP           - Show this help\n");
    kprint("APPSFS STATUS         - Show app runner status\n");
    kprint("APPFS HELP            - Alias for APPSFS HELP\n");
    kprint("RUNAPP FILE           - Run RAM file as app program\n");
    kprint("APPINFO FILE          - Show app file info\n");
    kprint("\nApp file command syntax uses semicolons:\n");
    kprint("PRINT TEXT            - Print text\n");
    kprint("RUN NAME ARGS         - Run internal program\n");
    kprint("EXEC NAME ARGS        - Alias for RUN\n");
    kprint("TICKS                 - Show timer ticks\n");
    kprint("UPTIME                - Show uptime seconds\n");
    kprint("PS                    - Show process history\n");
    kprint("PROGRAMS              - List internal programs\n");
    kprint("SYSCALL TEST          - Test safe syscall dispatcher\n");
    kprint("SYSCALL TICKS         - Show ticks through syscall\n");
    kprint("SYSCALL VERSION       - Show syscall ABI string\n");
    kprint("\nExample:\n");
    kprint("WRITE APP1 PRINT Hello; RUN HELLO Ahmad; TICKS; PS\n");
    kprint("RUNAPP APP1\n");
}

void appfs_status() {
    kprint("External RAM app runner status:\n");

    kprint("Initialized: ");
    if (appfs_ready) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }

    kprint("Total app runs: ");
    print_uint_local((u32)appfs_total_runs);
    kprint("\n");

    kprint("Last app: ");
    if (appfs_last_name[0] == '\0') {
        kprint("none\n");
    } else {
        kprint(appfs_last_name);
        kprint("\n");
    }

    kprint("Last command count: ");
    print_uint_local((u32)appfs_last_command_count);
    kprint("\n");

    kprint("Last result: ");
    if (appfs_last_result) {
        kprint("success\n");
    } else {
        kprint("failed or not run yet\n");
    }
}

void appfs_info(char *filename) {
    char name[RAMFS_NAME_SIZE];
    char *content;
    int size;
    int i;
    int commands = 0;
    int in_text = 0;

    read_word_local(filename, name, RAMFS_NAME_SIZE);

    if (name[0] == '\0') {
        kprint("Usage: APPINFO FILE\n");
        return;
    }

    content = ramfs_read(name);

    if (content == 0) {
        kprint("App file not found.\n");
        return;
    }

    size = ramfs_size(name);

    for (i = 0; content[i] != '\0'; i++) {
        if (!is_space(content[i]) && content[i] != ';') {
            in_text = 1;
        }

        if (content[i] == ';' && in_text) {
            commands++;
            in_text = 0;
        }
    }

    if (in_text) {
        commands++;
    }

    kprint("App file info:\n");
    kprint("Name: ");
    kprint(name);
    kprint("\n");

    kprint("Size: ");
    if (size >= 0) {
        print_uint_local((u32)size);
    } else {
        kprint("unknown");
    }
    kprint(" bytes\n");

    kprint("Estimated commands: ");
    print_uint_local((u32)commands);
    kprint("\n");

    kprint("Type: RAM-file text app\n");
}

static int appfs_run_syscall(char *args) {
    char subcommand[32];
    char *version;
    u32 result;

    args = read_word_local(args, subcommand, 32);
    args = skip_spaces_local(args);

    if (strcmp_ignore_case(subcommand, "TEST") == 0) {
        kprint("APP syscall test...\n");

        syscall_call1(SYS_PRINT, (u32)"SYS_PRINT: hello from RAM app syscall test\n");

        result = syscall_call0(SYS_GET_TICKS);
        kprint("SYS_GET_TICKS returned: ");
        print_uint_local(result);
        kprint("\n");

        version = (char *)syscall_call0(SYS_GET_VERSION);
        kprint("SYS_GET_VERSION returned: ");
        kprint(version);
        kprint("\n");

        return 1;
    }

    if (strcmp_ignore_case(subcommand, "TICKS") == 0) {
        result = syscall_call0(SYS_GET_TICKS);
        kprint("Ticks from syscall: ");
        print_uint_local(result);
        kprint("\n");
        return 1;
    }

    if (strcmp_ignore_case(subcommand, "VERSION") == 0) {
        version = (char *)syscall_call0(SYS_GET_VERSION);
        kprint("Syscall ABI: ");
        kprint(version);
        kprint("\n");
        return 1;
    }

    kprint("Unknown app SYSCALL command.\n");
    return 0;
}

static int appfs_run_command(char *command) {
    char op[32];
    char name[32];
    char *args;

    trim_right_local(command);
    command = skip_spaces_local(command);

    if (command[0] == '\0') {
        return 1;
    }

    args = read_word_local(command, op, 32);
    args = skip_spaces_local(args);

    kprint_colored("\n@app ", LIGHT_GREY_ON_BLACK);
    kprint_colored(command, LIGHT_GREY_ON_BLACK);
    kprint("\n");

    if (strcmp_ignore_case(op, "PRINT") == 0 ||
        strcmp_ignore_case(op, "ECHO") == 0) {
        kprint(args);
        kprint("\n");
        return 1;
    }

    if (strcmp_ignore_case(op, "RUN") == 0 ||
        strcmp_ignore_case(op, "EXEC") == 0 ||
        strcmp_ignore_case(op, "RUNPROG") == 0) {
        args = read_word_local(args, name, 32);
        args = skip_spaces_local(args);
        return program_run(name, args);
    }

    if (strcmp_ignore_case(op, "TICKS") == 0) {
        kprint("Ticks: ");
        print_uint_local(get_timer_ticks());
        kprint("\n");
        return 1;
    }

    if (strcmp_ignore_case(op, "UPTIME") == 0) {
        kprint("Uptime seconds: ");
        print_uint_local(get_timer_seconds());
        kprint("\n");
        return 1;
    }

    if (strcmp_ignore_case(op, "PS") == 0 ||
        strcmp_ignore_case(op, "PROCESS") == 0) {
        process_list();
        return 1;
    }

    if (strcmp_ignore_case(op, "PROGRAMS") == 0 ||
        strcmp_ignore_case(op, "PROGS") == 0) {
        program_list();
        return 1;
    }

    if (strcmp_ignore_case(op, "SYSCALL") == 0) {
        return appfs_run_syscall(args);
    }

    if (strcmp_ignore_case(op, "HELP") == 0) {
        appfs_help();
        return 1;
    }

    kprint("Unknown app command: ");
    kprint(op);
    kprint("\n");
    return 0;
}

int appfs_run(char *filename) {
    char name[RAMFS_NAME_SIZE];
    char script[APPFS_SCRIPT_SIZE];
    char command[APPFS_COMMAND_SIZE];
    char *content;
    int i = 0;
    int j = 0;
    int command_count = 0;
    int failed_count = 0;
    int result;

    if (!appfs_ready) {
        kprint("App runner is not initialized.\n");
        return 0;
    }

    read_word_local(filename, name, RAMFS_NAME_SIZE);

    if (name[0] == '\0') {
        kprint("Usage: RUNAPP FILE\n");
        return 0;
    }

    content = ramfs_read(name);

    if (content == 0) {
        kprint("App file not found.\n");
        appfs_last_result = 0;
        copy_string_limited_local(appfs_last_name, name, RAMFS_NAME_SIZE);
        return 0;
    }

    copy_string_limited_local(script, content, APPFS_SCRIPT_SIZE);
    copy_string_limited_local(appfs_last_name, name, RAMFS_NAME_SIZE);

    kprint("Running RAM app: ");
    kprint(name);
    kprint("\n");

    while (script[i] != '\0') {
        if (script[i] == ';') {
            command[j] = '\0';

            if (skip_spaces_local(command)[0] != '\0') {
                command_count++;
                result = appfs_run_command(command);

                if (!result) {
                    failed_count++;
                }
            }

            j = 0;
        } else {
            if (j < APPFS_COMMAND_SIZE - 1) {
                command[j] = script[i];
                j++;
            }
        }

        i++;
    }

    command[j] = '\0';

    if (skip_spaces_local(command)[0] != '\0') {
        command_count++;
        result = appfs_run_command(command);

        if (!result) {
            failed_count++;
        }
    }

    appfs_total_runs++;
    appfs_last_command_count = command_count;
    appfs_last_result = (failed_count == 0);

    kprint("RAM app finished: ");
    print_uint_local((u32)command_count);
    kprint(" commands, ");
    print_uint_local((u32)failed_count);
    kprint(" failed.\n");

    if (failed_count == 0) {
        return 1;
    }

    return 0;
}
