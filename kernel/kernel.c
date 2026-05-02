#include "../cpu/isr.h"
#include "../cpu/timer.h"
#include "../cpu/paging.h"
#include "../cpu/syscall.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/disk.h"
#include "../cpu/ports.h"
#include "kernel.h"
#include "programs.h"
#include "process.h"
#include "../libc/string.h"
#include "../libc/heap.h"
#include "../libc/ramfs.h"
#include "../libc/simplefs.h"

#define OS_VERSION "2.1"
#define INPUT_SIZE 256
#define SCRIPT_MAX_DEPTH 2

static int calculator_mode = 0;
static int notes_mode = 0;
static int files_mode = 0;
static int editor_mode = 0;
static int calc_last_answer = 0;
static unsigned int random_seed = 12345;
static char editor_filename[RAMFS_NAME_SIZE];
static int script_depth = 0;
static char script_last_name[RAMFS_NAME_SIZE];
static int script_last_command_count = 0;
static int script_last_result = 0;
static int script_total_runs = 0;
static u32 script_last_start_ticks = 0;
static u32 script_last_end_ticks = 0;

void user_input(char *input);

static char *skip_spaces_local(char *s) {
    while (*s != '\0' && is_space(*s)) {
        s++;
    }
    return s;
}

static void trim_right(char *s) {
    int len = strlen(s);

    while (len > 0 && is_space(s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static char *read_word(char *input, char word[], int max_len) {
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

static void copy_string_limited(char *dest, char *src, int max_len) {
    int i = 0;

    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

static void print_char(char c) {
    char s[2];

    s[0] = c;
    s[1] = '\0';
    kprint(s);
}

static void print_int(int value) {
    char str[32];

    int_to_ascii(value, str);
    kprint(str);
}

static void print_uint(u32 value) {
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

static char hex_digit(u32 value) {
    value = value & 0xF;

    if (value < 10) {
        return '0' + value;
    }

    return 'A' + (value - 10);
}

static void print_hex32(u32 value) {
    char str[11];
    int i;

    str[0] = '0';
    str[1] = 'x';

    for (i = 0; i < 8; i++) {
        str[2 + i] = hex_digit(value >> ((7 - i) * 4));
    }

    str[10] = '\0';
    kprint(str);
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static u32 parse_u32_auto(char *s, int *ok) {
    u32 result = 0;
    int value;

    s = skip_spaces_local(s);

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;

        if (hex_value(*s) < 0) {
            *ok = 0;
            return 0;
        }

        while (hex_value(*s) >= 0) {
            value = hex_value(*s);
            result = result * 16 + value;
            s++;
        }

        s = skip_spaces_local(s);

        if (*s != '\0') {
            *ok = 0;
            return 0;
        }

        *ok = 1;
        return result;
    }

    if (!is_digit(*s)) {
        *ok = 0;
        return 0;
    }

    while (is_digit(*s)) {
        result = result * 10 + (*s - '0');
        s++;
    }

    s = skip_spaces_local(s);

    if (*s != '\0') {
        *ok = 0;
        return 0;
    }

    *ok = 1;
    return result;
}

static char *parse_int(char *s, int *value, int *ok) {
    int sign = 1;
    int result = 0;

    s = skip_spaces_local(s);

    if (*s == '-') {
        sign = -1;
        s++;
    }

    if (!is_digit(*s)) {
        *ok = 0;
        return s;
    }

    while (is_digit(*s)) {
        result = result * 10 + (*s - '0');
        s++;
    }

    *value = result * sign;
    *ok = 1;

    return s;
}

static int parse_two_ints(char *args, int *a, int *b) {
    int ok_a;
    int ok_b;
    char *p;

    p = parse_int(args, a, &ok_a);

    if (!ok_a) {
        return 0;
    }

    p = parse_int(p, b, &ok_b);

    if (!ok_b) {
        return 0;
    }

    p = skip_spaces_local(p);

    if (*p != '\0') {
        return 0;
    }

    return 1;
}

static int parse_calculator_expression(char *input, int *a, char *op, int *b) {
    int ok_a;
    int ok_b;
    char *p;

    p = parse_int(input, a, &ok_a);

    if (!ok_a) {
        return 0;
    }

    p = skip_spaces_local(p);

    if (*p != '+' && *p != '-' && *p != '*' && *p != '/' && *p != '%') {
        return 0;
    }

    *op = *p;
    p++;

    p = parse_int(p, b, &ok_b);

    if (!ok_b) {
        return 0;
    }

    p = skip_spaces_local(p);

    if (*p != '\0') {
        return 0;
    }

    return 1;
}

static void print_prompt() {
    if (calculator_mode) {
        kprint("\ncalc> ");
    } else if (notes_mode) {
        kprint("\nnotes> ");
    } else if (files_mode) {
        kprint("\nfiles> ");
    } else if (editor_mode) {
        kprint("\nedit> ");
    } else {
        kprint("\n> ");
    }
}

static void halt_cpu() {
    kprint("Stopping the CPU. Bye!\n");

    asm volatile("cli");

    while (1) {
        asm volatile("hlt");
    }
}

static void reboot() {
    unsigned char status;

    kprint("Rebooting...\n");

    do {
        status = port_byte_in(0x64);
    } while (status & 0x02);

    port_byte_out(0x64, 0xFE);

    halt_cpu();
}

static void print_banner() {
    kprint_colored("Simple_OS / MyOS\n", LIGHT_CYAN_ON_BLACK);
    kprint_colored("32-bit x86 protected mode CLI OS\n", LIGHT_GREEN_ON_BLACK);
}

static void print_calculator_help() {
    kprint_colored("Calculator app mode\n", YELLOW_ON_BLACK);
    kprint("Use: NUMBER OP NUMBER\n");
    kprint("Examples: 5 + 3, 10 - 4, 6 * 7, 20 / 5, 10 % 3\n");
    kprint("Commands: HELP, ANS, CLEAR, EXIT\n");
}

static void print_notes_help() {
    kprint_colored("Notes app mode\n", YELLOW_ON_BLACK);
    kprint("SAVE NAME TEXT, OPEN NAME, APPEND NAME TEXT, DELETE NAME\n");
    kprint("LIST, FS, CLEAR, HELP, EXIT\n");
}

static void print_files_help() {
    kprint_colored("Files app mode\n", YELLOW_ON_BLACK);
    kprint("LS, LS -L, TOUCH FILE, WRITE FILE TEXT, APPEND FILE TEXT\n");
    kprint("CAT FILE, EDIT FILE, RUNFILE FILE, SIZE FILE, RENAME OLD NEW\n");
    kprint("COPY SRC DST, EXISTS FILE, CLEARFILE FILE, RM FILE\n");
    kprint("DISKINFO, FORMATFS, SAVEFS, LOADFS, FS, CLEAR, HELP, EXIT\n");
}

static void print_editor_help() {
    kprint_colored("Editor app mode\n", YELLOW_ON_BLACK);
    kprint(".SHOW, .SAVE TEXT, .ADD TEXT, .APPEND TEXT, .CLEAR, .INFO, .HELP, .EXIT\n");
}

static void print_apps() {
    kprint_colored("Built-in app modes:\n", YELLOW_ON_BLACK);
    kprint("CALC   - Calculator app\n");
    kprint("NOTES  - Notes app using RAM filesystem\n");
    kprint("FILES  - File manager app using RAM filesystem\n");
    kprint("EDITOR - Text editor app\n");
    kprint("Run with: RUN CALC, RUN NOTES, RUN FILES, RUN EDITOR FILE\n");
}

static void print_help() {
    kprint_colored("Available commands:\n", YELLOW_ON_BLACK);

    kprint("HELP, APPS, RUN APP, CLEAR, VERSION, ABOUT, BANNER, SYSINFO\n");
    kprint("PROGRAMS, PROGS, PROGRAM HELP, PROGRAM COUNT, PROGRAM STATUS\n");
    kprint("PROGRAM INFO NAME, PROGRAM INFO ALL, RUNPROG NAME, EXEC NAME\n");
    kprint("PS, PROCESS HELP, PROCESS LIST, PROCESS STATUS, PROCESS COUNT, PROCESS CLEAR\n");
    kprint("SYSCALLS, SYSTEMCALLS, SYSCALL HELP/TEST/PRINT/TICKS/VERSION/CLEAR/INVALID\n");
    kprint("TICKS, UPTIME, MEMORY, MEMMAP, PAGING, PAGE ADDRESS\n");
    kprint("HEAP, HEAP BLOCKS, HEAP RESET, ALLOC N, FREE ADDRESS\n");
    kprint("DISKINFO, FORMATFS, SAVEFS, LOADFS\n");
    kprint("FS, FS RESET, FS SAVE, FS LOAD, FS FORMAT, LS, LS -L, TOUCH, WRITE, APPEND, CAT\n");
    kprint("SCRIPT HELP/STATUS/DEMO/RUN FILE, RUNFILE FILE, EDIT\n");
    kprint("SIZE, RENAME, COPY, EXISTS, CLEARFILE, RM\n");
    kprint("NOTE LIST/SAVE/OPEN/APPEND/DELETE, COLOR NAME\n");
    kprint("CALC, NOTES, FILES, EDITOR FILE\n");
    kprint("ECHO, UPPER, LOWER, REVERSE, LEN, ASCII, REPEAT, COUNT, RAND\n");
    kprint("ADD, SUB, MUL, DIV, MOD, REBOOT, HALT, END\n");
}

static void command_echo(char *args) {
    kprint(args);
    kprint("\n");
}

static void command_upper(char *args) {
    int i = 0;

    while (args[i] != '\0') {
        print_char(to_upper(args[i]));
        i++;
    }

    kprint("\n");
}

static void command_lower(char *args) {
    int i = 0;

    while (args[i] != '\0') {
        print_char(to_lower(args[i]));
        i++;
    }

    kprint("\n");
}

static void command_reverse(char *args) {
    int i;

    i = strlen(args) - 1;

    while (i >= 0) {
        print_char(args[i]);
        i--;
    }

    kprint("\n");
}

static void command_len(char *args) {
    print_int(strlen(args));
    kprint("\n");
}

static void command_ascii(char *args) {
    if (args[0] == '\0') {
        kprint("Usage: ASCII C\n");
        return;
    }

    print_int((int)args[0]);
    kprint("\n");
}

static void command_repeat(char *args) {
    int count;
    int ok;
    int i;
    char *text;

    text = parse_int(args, &count, &ok);
    text = skip_spaces_local(text);

    if (!ok || count < 0) {
        kprint("Usage: REPEAT N TEXT\n");
        return;
    }

    if (count > 20) {
        kprint("Limit: max repeat is 20\n");
        return;
    }

    for (i = 0; i < count; i++) {
        kprint(text);
        kprint("\n");
    }
}

static void command_count(char *args) {
    int count;
    int ok;
    int i;

    parse_int(args, &count, &ok);

    if (!ok || count < 1) {
        kprint("Usage: COUNT N\n");
        return;
    }

    if (count > 100) {
        kprint("Limit: max count is 100\n");
        return;
    }

    for (i = 1; i <= count; i++) {
        print_int(i);
        kprint(" ");
    }

    kprint("\n");
}

static void command_rand() {
    random_seed = random_seed * 1103515245 + 12345;
    print_uint((random_seed / 65536) % 32768);
    kprint("\n");
}

static void command_math(char *cmd, char *args) {
    int a;
    int b;
    int result;

    if (!parse_two_ints(args, &a, &b)) {
        kprint("Usage: ");
        kprint(cmd);
        kprint(" A B\n");
        return;
    }

    if (strcmp_ignore_case(cmd, "ADD") == 0) {
        result = a + b;
    } else if (strcmp_ignore_case(cmd, "SUB") == 0) {
        result = a - b;
    } else if (strcmp_ignore_case(cmd, "MUL") == 0) {
        result = a * b;
    } else if (strcmp_ignore_case(cmd, "DIV") == 0) {
        if (b == 0) {
            kprint("Error: division by zero\n");
            return;
        }
        result = a / b;
    } else if (strcmp_ignore_case(cmd, "MOD") == 0) {
        if (b == 0) {
            kprint("Error: modulo by zero\n");
            return;
        }
        result = a % b;
    } else {
        kprint("Math command error\n");
        return;
    }

    print_int(result);
    kprint("\n");
}

static void command_syscalls() {
    int i;

    kprint("System call table:\n");
    kprint("Mode: ");
    kprint(syscall_get_mode());
    kprint("\n");
    kprint("Real int 0x80: disabled for now\n");
    kprint("Dispatcher enabled: ");

    if (syscall_is_enabled()) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }

    for (i = 1; i <= syscall_get_count(); i++) {
        print_int(i);
        kprint(" - ");
        kprint(syscall_get_name((u32)i));
        kprint(" - ");
        kprint(syscall_get_description((u32)i));
        kprint("\n");
    }
}

static void command_syscall(char *args) {
    char subcommand[32];
    char *text;
    u32 result;
    char *version;

    args = read_word(args, subcommand, 32);
    args = skip_spaces_local(args);

    if (subcommand[0] == '\0' || strcmp_ignore_case(subcommand, "HELP") == 0) {
        kprint("SYSCALL TEST, PRINT TEXT, TICKS, VERSION, CLEAR, INVALID\n");
        kprint("SYSCALLS and SYSTEMCALLS show the syscall table.\n");
        return;
    }

    if (strcmp_ignore_case(subcommand, "TEST") == 0) {
        kprint("Testing safe syscall dispatcher...\n");
        syscall_call1(SYS_PRINT, (u32)"SYS_PRINT: hello from safe syscall dispatcher\n");

        result = syscall_call0(SYS_GET_TICKS);
        kprint("SYS_GET_TICKS returned: ");
        print_uint(result);
        kprint("\n");

        version = (char *)syscall_call0(SYS_GET_VERSION);
        kprint("SYS_GET_VERSION returned: ");
        kprint(version);
        kprint("\n");
        kprint("Syscall test complete.\n");
        return;
    }

    if (strcmp_ignore_case(subcommand, "PRINT") == 0) {
        text = args;

        if (text[0] == '\0') {
            kprint("Usage: SYSCALL PRINT TEXT\n");
            return;
        }

        result = syscall_call1(SYS_PRINT, (u32)text);
        kprint("\nSYS_PRINT returned length: ");
        print_uint(result);
        kprint("\n");
        return;
    }

    if (strcmp_ignore_case(subcommand, "TICKS") == 0) {
        result = syscall_call0(SYS_GET_TICKS);
        kprint("Ticks from syscall: ");
        print_uint(result);
        kprint("\n");
        return;
    }

    if (strcmp_ignore_case(subcommand, "VERSION") == 0) {
        version = (char *)syscall_call0(SYS_GET_VERSION);
        kprint("Syscall ABI: ");
        kprint(version);
        kprint("\n");
        return;
    }

    if (strcmp_ignore_case(subcommand, "CLEAR") == 0) {
        syscall_call0(SYS_CLEAR);
        kprint("Screen cleared by syscall dispatcher.\n");
        return;
    }

    if (strcmp_ignore_case(subcommand, "INVALID") == 0) {
        result = syscall_call0(999);
        kprint("Invalid syscall returned: ");
        print_hex32(result);
        kprint("\n");
        return;
    }

    kprint("Unknown syscall command. Use SYSCALL HELP.\n");
}

static void command_sysinfo() {
    kprint("OS name: Simple_OS / MyOS\n");
    kprint("Version: ");
    kprint(OS_VERSION);
    kprint("\n");
    kprint("Architecture: 32-bit x86\n");
    kprint("Mode: Protected mode\n");
    kprint("Display: VGA text mode\n");
    kprint("Keyboard: PS/2 IRQ1\n");
    kprint("Timer frequency: 50 Hz\n");
    kprint("Heap: v2 block allocator\n");
    kprint("Paging: identity paging enabled\n");
    kprint("System calls: safe syscall dispatcher enabled\n");
    kprint("Real int 0x80: disabled for now\n");
    kprint("RAM filesystem: enabled\n");
    kprint("Persistent storage: SimpleFS on QEMU IDE disk\n");
    kprint("App manager/editor/script runner: enabled\n");
    kprint("Internal program manager: v2 enabled\n");
    kprint("Process manager: v1 history tracking enabled\n");
}

static void command_memory() {
    kprint("Memory info:\n");
    kprint("Kernel load address: 0x10000\n");
    kprint("VGA text memory: 0xB8000\n");
    kprint("Boot stack: 0x90000\n");
    kprint("Heap start: ");
    print_hex32(heap_get_start());
    kprint("\nHeap end: ");
    print_hex32(heap_get_end());
    kprint("\nPaging: identity mapped first 4 MB\n");
    kprint("Syscall mode: safe kernel dispatcher\n");
}

static void command_memmap() {
    kprint("Memory map:\n");
    kprint("Bootloader:       0x00007C00\n");
    kprint("Kernel load:      0x00010000\n");
    kprint("Heap start:       ");
    print_hex32(heap_get_start());
    kprint("\nHeap end:         ");
    print_hex32(heap_get_end());
    kprint("\nVGA text memory:  0x000B8000\n");
    kprint("Boot stack:       0x00090000\n");
    kprint("Paging map:       0x00000000 - 0x003FFFFF identity mapped\n");
    kprint("SimpleFS disk:    LBA 2048 - LBA 2056\n");
}

static void command_paging() {
    kprint("Paging status:\n");
    kprint("Enabled: ");

    if (paging_is_enabled()) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }

    kprint("Page directory: ");
    print_hex32(paging_get_directory_address());
    kprint("\nFirst page table: ");
    print_hex32(paging_get_first_table_address());
    kprint("\nMapped range: 0x00000000 - 0x003FFFFF\n");
    kprint("Mapping type: identity paging\n");
}

static void command_page(char *args) {
    u32 address;
    int ok;

    address = parse_u32_auto(args, &ok);

    if (!ok) {
        kprint("Usage: PAGE ADDRESS\nExample: PAGE 0x000B8000\n");
        return;
    }

    kprint("Address: ");
    print_hex32(address);
    kprint("\nDirectory index: ");
    print_uint(paging_get_directory_index(address));
    kprint("\nTable index: ");
    print_uint(paging_get_table_index(address));
    kprint("\nPage offset: ");
    print_hex32(paging_get_page_offset(address));
    kprint("\nMapped: ");

    if (paging_is_mapped(address)) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }
}

static void command_heap_blocks() {
    int i;
    int count;
    int is_free;
    u32 address;
    u32 size;

    count = heap_get_block_count();
    kprint("Heap blocks:\n");
    kprint("Address      Size      State\n");

    for (i = 0; i < count; i++) {
        if (heap_get_block_info(i, &address, &size, &is_free)) {
            print_hex32(address);
            kprint("  ");
            print_uint(size);
            kprint("  ");

            if (is_free) {
                kprint("FREE\n");
            } else {
                kprint("USED\n");
            }
        }
    }
}

static void command_heap(char *args) {
    args = skip_spaces_local(args);

    if (strcmp_ignore_case(args, "RESET") == 0) {
        heap_reset();
        kprint("Heap reset.\n");
        return;
    }

    if (strcmp_ignore_case(args, "BLOCKS") == 0 || strcmp_ignore_case(args, "BLOCK") == 0) {
        command_heap_blocks();
        return;
    }

    kprint("Heap info:\nStart:   ");
    print_hex32(heap_get_start());
    kprint("\nEnd:     ");
    print_hex32(heap_get_end());
    kprint("\nTotal:   ");
    print_uint(heap_get_total());
    kprint(" bytes\nUsed:    ");
    print_uint(heap_get_used());
    kprint(" bytes\nFree:    ");
    print_uint(heap_get_free());
    kprint(" bytes\nBlocks:  ");
    print_int(heap_get_block_count());
    kprint("\n");
}

static void command_alloc(char *args) {
    int bytes;
    int ok;
    void *ptr;

    parse_int(args, &bytes, &ok);

    if (!ok || bytes <= 0) {
        kprint("Usage: ALLOC N\n");
        return;
    }

    if (bytes > 65536) {
        kprint("Limit: max allocation is 65536 bytes\n");
        return;
    }

    ptr = heap_alloc((u32)bytes);

    if (ptr == 0) {
        kprint("Heap allocation failed.\n");
        return;
    }

    kprint("Allocated ");
    print_int(bytes);
    kprint(" bytes at ");
    print_hex32((u32)ptr);
    kprint("\n");
}

static void command_free(char *args) {
    int ok;
    u32 address;

    address = parse_u32_auto(args, &ok);

    if (!ok || address == 0) {
        kprint("Usage: FREE ADDRESS\nExample: FREE 0x0010000C\n");
        return;
    }

    if (heap_free((void *)address)) {
        kprint("Heap block freed.\n");
    } else {
        kprint("Free failed. Invalid address or block already free.\n");
    }
}

static void command_diskinfo() {
    int count;

    kprint("Disk info:\nDriver: ATA PIO primary IDE\nQEMU disk image: disk.img\n");
    kprint("Disk present: ");

    if (disk_is_present()) {
        kprint("yes\n");
    } else {
        kprint("no\n");
    }

    kprint("Last disk status: ");
    kprint(disk_get_status_text());
    kprint("\nSimpleFS start LBA: ");
    print_uint(simplefs_get_start_lba());
    kprint("\nSimpleFS sectors: ");
    print_uint(simplefs_get_sector_count());
    kprint("\nSimpleFS formatted: ");

    if (simplefs_is_formatted()) {
        kprint("yes\nFiles on disk: ");
        count = simplefs_get_disk_file_count();

        if (count >= 0) {
            print_int(count);
        } else {
            kprint("unknown");
        }

        kprint("\n");
    } else {
        kprint("no\n");
    }
}

static void command_formatfs() {
    if (simplefs_format()) {
        kprint("SimpleFS formatted on disk.img.\n");
    } else {
        kprint("Format failed. Disk status: ");
        kprint(disk_get_status_text());
        kprint("\n");
    }
}

static void command_savefs() {
    if (simplefs_save()) {
        kprint("RAM filesystem saved to disk.img.\n");
    } else {
        kprint("SaveFS failed. Disk status: ");
        kprint(disk_get_status_text());
        kprint("\n");
    }
}

static void command_loadfs() {
    int result;

    result = simplefs_load();

    if (result == 1) {
        kprint("RAM filesystem loaded from disk.img.\n");
    } else if (result == -2) {
        kprint("SimpleFS is not formatted. Use FORMATFS first.\n");
    } else if (result == -3) {
        kprint("Unsupported SimpleFS version.\n");
    } else {
        kprint("LoadFS failed. Disk status: ");
        kprint(disk_get_status_text());
        kprint("\n");
    }
}

static void command_fs(char *args) {
    args = skip_spaces_local(args);

    if (strcmp_ignore_case(args, "RESET") == 0) {
        ramfs_reset();
        kprint("RAM filesystem reset.\n");
        return;
    }

    if (strcmp_ignore_case(args, "SAVE") == 0 ||
        strcmp_ignore_case(args, "WRITE") == 0) {
        command_savefs();
        return;
    }

    if (strcmp_ignore_case(args, "LOAD") == 0 ||
        strcmp_ignore_case(args, "READ") == 0) {
        command_loadfs();
        return;
    }

    if (strcmp_ignore_case(args, "FORMAT") == 0 ||
        strcmp_ignore_case(args, "FORMATFS") == 0) {
        command_formatfs();
        return;
    }

    if (strcmp_ignore_case(args, "HELP") == 0) {
        kprint("FS command help:\n");
        kprint("FS              - Show RAM filesystem info\n");
        kprint("FS RESET        - Clear RAM files\n");
        kprint("FS SAVE         - Save RAM files to disk.img\n");
        kprint("FS LOAD         - Load RAM files from disk.img\n");
        kprint("FS FORMAT       - Format SimpleFS area on disk.img\n");
        kprint("LS / LS -L      - List RAM files\n");
        kprint("WRITE FILE TXT  - Write RAM file\n");
        kprint("RUNFILE FILE    - Run script file\n");
        return;
    }

    kprint("RAM filesystem info:\n");
    kprint("Max files: ");
    print_int(RAMFS_MAX_FILES);
    kprint("\n");
    kprint("Filename size: ");
    print_int(RAMFS_NAME_SIZE - 1);
    kprint(" chars\n");
    kprint("File content size: ");
    print_int(RAMFS_CONTENT_SIZE - 1);
    kprint(" chars\n");
    kprint("Files used: ");
    print_int(ramfs_file_count());
    kprint("\n");
    kprint("Persistent commands: FS SAVE, FS LOAD, FS FORMAT\n");
}

static void command_ls_short() {
    int i;
    int count;
    char name[RAMFS_NAME_SIZE];

    count = ramfs_file_count();

    if (count == 0) {
        kprint("No files.\n");
        return;
    }

    for (i = 0; i < count; i++) {
        if (ramfs_get_file_name(i, name, RAMFS_NAME_SIZE)) {
            kprint(name);
            kprint("\n");
        }
    }
}

static void command_ls_long() {
    int i;
    int count;
    int size;
    char name[RAMFS_NAME_SIZE];

    count = ramfs_file_count();

    if (count == 0) {
        kprint("No files.\n");
        return;
    }

    kprint("Name            Size\n");

    for (i = 0; i < count; i++) {
        if (ramfs_get_file_name(i, name, RAMFS_NAME_SIZE)) {
            size = ramfs_size(name);
            kprint(name);
            kprint("    ");
            print_int(size);
            kprint(" bytes\n");
        }
    }
}

static void command_ls(char *args) {
    args = skip_spaces_local(args);

    if (strcmp_ignore_case(args, "-L") == 0 || strcmp_ignore_case(args, "/L") == 0 || strcmp_ignore_case(args, "LONG") == 0) {
        command_ls_long();
        return;
    }

    command_ls_short();
}

static void command_touch(char *args) {
    char filename[RAMFS_NAME_SIZE];
    int result;

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: TOUCH FILE\n");
        return;
    }

    result = ramfs_create(filename);

    if (result == 1) {
        kprint("File created.\n");
    } else if (result == -1) {
        kprint("Invalid filename.\n");
    } else if (result == -2) {
        kprint("File already exists.\n");
    } else if (result == -3) {
        kprint("RAM filesystem full.\n");
    } else {
        kprint("File create error.\n");
    }
}

static void command_write(char *args) {
    char filename[RAMFS_NAME_SIZE];
    char *content;
    int result;

    args = read_word(args, filename, RAMFS_NAME_SIZE);
    content = skip_spaces_local(args);

    if (filename[0] == '\0') {
        kprint("Usage: WRITE FILE TEXT\n");
        return;
    }

    if (!ramfs_exists(filename)) {
        result = ramfs_create(filename);

        if (result != 1) {
            kprint("Could not create file.\n");
            return;
        }
    }

    result = ramfs_write(filename, content);

    if (result == 1) {
        kprint("File written.\n");
    } else if (result == -1) {
        kprint("File not found.\n");
    } else if (result == -2) {
        kprint("Content too large.\n");
    } else {
        kprint("Write error.\n");
    }
}

static void command_append(char *args) {
    char filename[RAMFS_NAME_SIZE];
    char *content;
    int result;

    args = read_word(args, filename, RAMFS_NAME_SIZE);
    content = skip_spaces_local(args);

    if (filename[0] == '\0') {
        kprint("Usage: APPEND FILE TEXT\n");
        return;
    }

    result = ramfs_append(filename, content);

    if (result == 1) {
        kprint("File appended.\n");
    } else if (result == -1) {
        kprint("File not found.\n");
    } else if (result == -2) {
        kprint("File content limit reached.\n");
    } else {
        kprint("Append error.\n");
    }
}

static void command_cat(char *args) {
    char filename[RAMFS_NAME_SIZE];
    char *content;

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: CAT FILE\n");
        return;
    }

    content = ramfs_read(filename);

    if (content == 0) {
        kprint("File not found.\n");
        return;
    }

    kprint(content);
    kprint("\n");
}

static void command_rm(char *args) {
    char filename[RAMFS_NAME_SIZE];

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: RM FILE\n");
        return;
    }

    if (ramfs_delete(filename) == 1) {
        kprint("File deleted.\n");
    } else {
        kprint("File not found.\n");
    }
}

static void command_size(char *args) {
    char filename[RAMFS_NAME_SIZE];
    int size;

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: SIZE FILE\n");
        return;
    }

    size = ramfs_size(filename);

    if (size < 0) {
        kprint("File not found.\n");
        return;
    }

    kprint(filename);
    kprint(": ");
    print_int(size);
    kprint(" bytes\n");
}

static void command_rename(char *args) {
    char old_name[RAMFS_NAME_SIZE];
    char new_name[RAMFS_NAME_SIZE];
    int result;

    args = read_word(args, old_name, RAMFS_NAME_SIZE);
    args = read_word(args, new_name, RAMFS_NAME_SIZE);

    if (old_name[0] == '\0' || new_name[0] == '\0') {
        kprint("Usage: RENAME OLD NEW\n");
        return;
    }

    result = ramfs_rename(old_name, new_name);

    if (result == 1) {
        kprint("File renamed.\n");
    } else if (result == -1) {
        kprint("Source file not found.\n");
    } else if (result == -2) {
        kprint("Invalid new filename.\n");
    } else if (result == -3) {
        kprint("Destination filename already exists.\n");
    } else {
        kprint("Rename error.\n");
    }
}

static void command_copy(char *args) {
    char src_name[RAMFS_NAME_SIZE];
    char dst_name[RAMFS_NAME_SIZE];
    int result;

    args = read_word(args, src_name, RAMFS_NAME_SIZE);
    args = read_word(args, dst_name, RAMFS_NAME_SIZE);

    if (src_name[0] == '\0' || dst_name[0] == '\0') {
        kprint("Usage: COPY SRC DST\n");
        return;
    }

    result = ramfs_copy(src_name, dst_name);

    if (result == 1) {
        kprint("File copied.\n");
    } else if (result == -1) {
        kprint("Source file not found.\n");
    } else if (result == -2) {
        kprint("Invalid destination filename.\n");
    } else if (result == -3) {
        kprint("Destination file already exists.\n");
    } else if (result == -4) {
        kprint("RAM filesystem full.\n");
    } else {
        kprint("Copy error.\n");
    }
}

static void command_exists(char *args) {
    char filename[RAMFS_NAME_SIZE];

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: EXISTS FILE\n");
        return;
    }

    if (ramfs_exists(filename)) {
        kprint("File exists.\n");
    } else {
        kprint("File does not exist.\n");
    }
}

static void command_clearfile(char *args) {
    char filename[RAMFS_NAME_SIZE];

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: CLEARFILE FILE\n");
        return;
    }

    if (ramfs_clear(filename) == 1) {
        kprint("File cleared.\n");
    } else {
        kprint("File not found.\n");
    }
}

static void editor_show() {
    char *content;

    content = ramfs_read(editor_filename);

    if (content == 0) {
        kprint("Editor error: file not found.\n");
        return;
    }

    if (content[0] == '\0') {
        kprint("[empty file]\n");
        return;
    }

    kprint(content);
    kprint("\n");
}

static void editor_info() {
    int size;

    size = ramfs_size(editor_filename);

    kprint("Editing file: ");
    kprint(editor_filename);
    kprint("\nSize: ");

    if (size < 0) {
        kprint("file not found\n");
    } else {
        print_int(size);
        kprint(" bytes\n");
    }
}

static void editor_save(char *text) {
    int result;

    result = ramfs_write(editor_filename, text);

    if (result == 1) {
        kprint("File saved.\n");
    } else if (result == -2) {
        kprint("Text too large.\n");
    } else {
        kprint("Save failed.\n");
    }
}

static void editor_add(char *text) {
    int result;

    result = ramfs_append(editor_filename, text);

    if (result == 1) {
        kprint("Text appended.\n");
    } else if (result == -2) {
        kprint("File content limit reached.\n");
    } else {
        kprint("Append failed.\n");
    }
}

static void editor_clear_file() {
    if (ramfs_clear(editor_filename) == 1) {
        kprint("File cleared.\n");
    } else {
        kprint("Clear failed.\n");
    }
}

static void start_editor(char *args) {
    char filename[RAMFS_NAME_SIZE];
    int result;

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: EDIT FILE\n");
        return;
    }

    if (!ramfs_exists(filename)) {
        result = ramfs_create(filename);

        if (result != 1) {
            if (result == -1) {
                kprint("Invalid filename.\n");
            } else if (result == -3) {
                kprint("RAM filesystem full.\n");
            } else {
                kprint("Could not create file.\n");
            }
            return;
        }
        kprint("New file created.\n");
    }

    editor_mode = 1;
    calculator_mode = 0;
    notes_mode = 0;
    files_mode = 0;
    strcpy(editor_filename, filename);

    kprint("Editing file: ");
    kprint(editor_filename);
    kprint("\n");
    print_editor_help();
}

static void note_list() {
    command_ls("");
}

static void note_save(char *args) {
    command_write(args);
}

static void note_open(char *args) {
    command_cat(args);
}

static void note_append(char *args) {
    command_append(args);
}

static void note_delete(char *args) {
    command_rm(args);
}

static void command_note(char *args) {
    char subcommand[32];

    args = read_word(args, subcommand, 32);
    args = skip_spaces_local(args);

    if (subcommand[0] == '\0' || strcmp_ignore_case(subcommand, "HELP") == 0) {
        print_notes_help();
    } else if (strcmp_ignore_case(subcommand, "LIST") == 0 || strcmp_ignore_case(subcommand, "LS") == 0) {
        note_list();
    } else if (strcmp_ignore_case(subcommand, "SAVE") == 0 || strcmp_ignore_case(subcommand, "WRITE") == 0) {
        note_save(args);
    } else if (strcmp_ignore_case(subcommand, "OPEN") == 0 || strcmp_ignore_case(subcommand, "SHOW") == 0 || strcmp_ignore_case(subcommand, "READ") == 0) {
        note_open(args);
    } else if (strcmp_ignore_case(subcommand, "APPEND") == 0) {
        note_append(args);
    } else if (strcmp_ignore_case(subcommand, "DELETE") == 0 || strcmp_ignore_case(subcommand, "RM") == 0 || strcmp_ignore_case(subcommand, "DEL") == 0) {
        note_delete(args);
    } else {
        kprint("Unknown NOTE command. Use NOTE HELP.\n");
    }
}

static void command_color(char *args) {
    if (strcmp_ignore_case(args, "WHITE") == 0) {
        set_text_color(WHITE_ON_BLACK);
    } else if (strcmp_ignore_case(args, "GREEN") == 0) {
        set_text_color(LIGHT_GREEN_ON_BLACK);
    } else if (strcmp_ignore_case(args, "CYAN") == 0) {
        set_text_color(LIGHT_CYAN_ON_BLACK);
    } else if (strcmp_ignore_case(args, "RED") == 0) {
        set_text_color(LIGHT_RED_ON_BLACK);
    } else if (strcmp_ignore_case(args, "YELLOW") == 0) {
        set_text_color(YELLOW_ON_BLACK);
    } else if (strcmp_ignore_case(args, "BLUE") == 0) {
        set_text_color(LIGHT_BLUE_ON_BLACK);
    } else if (strcmp_ignore_case(args, "MAGENTA") == 0) {
        set_text_color(LIGHT_MAGENTA_ON_BLACK);
    } else if (strcmp_ignore_case(args, "GREY") == 0 || strcmp_ignore_case(args, "GRAY") == 0) {
        set_text_color(LIGHT_GREY_ON_BLACK);
    } else {
        kprint("Usage: COLOR WHITE|GREEN|CYAN|RED|YELLOW|BLUE|MAGENTA|GREY\n");
        return;
    }

    kprint("Color changed.\n");
}

static void run_script_command(char command[]) {
    char local_command[INPUT_SIZE];

    trim_right(command);
    command = skip_spaces_local(command);

    if (command[0] == '\0') {
        return;
    }

    if (command[0] == '#') {
        return;
    }

    copy_string_limited(local_command, command, INPUT_SIZE);

    script_last_command_count++;

    kprint_colored("\n$ ", LIGHT_GREY_ON_BLACK);
    kprint_colored(local_command, LIGHT_GREY_ON_BLACK);
    kprint("\n");

    user_input(local_command);
}

static void command_runfile(char *args) {
    char filename[RAMFS_NAME_SIZE];
    char script[RAMFS_CONTENT_SIZE];
    char command[INPUT_SIZE];
    char *content;
    int i = 0;
    int j = 0;

    args = read_word(args, filename, RAMFS_NAME_SIZE);

    if (filename[0] == '\0') {
        kprint("Usage: RUNFILE FILE\n");
        script_last_result = 0;
        return;
    }

    if (script_depth >= SCRIPT_MAX_DEPTH) {
        kprint("Script nesting limit reached.\n");
        script_last_result = 0;
        return;
    }

    content = ramfs_read(filename);

    if (content == 0) {
        kprint("Script file not found.\n");
        copy_string_limited(script_last_name, filename, RAMFS_NAME_SIZE);
        script_last_command_count = 0;
        script_last_result = 0;
        return;
    }

    copy_string_limited(script, content, RAMFS_CONTENT_SIZE);
    copy_string_limited(script_last_name, filename, RAMFS_NAME_SIZE);

    script_last_command_count = 0;
    script_last_result = 0;
    script_last_start_ticks = get_timer_ticks();

    kprint("Running script: ");
    kprint(filename);
    kprint("\n");

    script_depth++;

    while (script[i] != '\0') {
        if (script[i] == ';' || script[i] == '\n') {
            command[j] = '\0';
            run_script_command(command);
            j = 0;
        } else if (j < INPUT_SIZE - 1) {
            command[j] = script[i];
            j++;
        }
        i++;
    }

    command[j] = '\0';
    run_script_command(command);

    script_depth--;

    script_last_end_ticks = get_timer_ticks();
    script_last_result = 1;
    script_total_runs++;

    kprint("Script finished. Commands executed: ");
    print_int(script_last_command_count);
    kprint("\n");
}

static void command_script_status() {
    kprint("Script runner status:\n");

    kprint("Last script: ");
    if (script_last_name[0] == '\0') {
        kprint("none\n");
    } else {
        kprint(script_last_name);
        kprint("\n");
    }

    kprint("Last result: ");
    if (script_last_result) {
        kprint("success\n");
    } else {
        kprint("none/failed\n");
    }

    kprint("Last command count: ");
    print_int(script_last_command_count);
    kprint("\n");

    kprint("Total script runs: ");
    print_int(script_total_runs);
    kprint("\n");

    kprint("Last start ticks: ");
    print_uint(script_last_start_ticks);
    kprint("\n");

    kprint("Last end ticks: ");
    print_uint(script_last_end_ticks);
    kprint("\n");
}

static void command_script_help() {
    kprint("Script command help:\n");
    kprint("RUNFILE FILE          - Run semicolon/newline separated commands from RAM file\n");
    kprint("SCRIPT FILE           - Alias for RUNFILE FILE\n");
    kprint("SCRIPT RUN FILE       - Run script file\n");
    kprint("SCRIPT STATUS         - Show script runner status\n");
    kprint("SCRIPT DEMO           - Create demo script file named demo\n");
    kprint("SCRIPT HELP           - Show this help\n");
    kprint("\nExample:\n");
    kprint("WRITE demo VERSION; PROGRAMS; RUNPROG HELLO Ahmad; RUNPROG CLOCK; PS\n");
    kprint("RUNFILE demo\n");
}

static void command_script_demo() {
    int result;

    result = ramfs_write("demo", "version; sysinfo; programs; runprog hello Ahmad; runprog clock; ps");

    if (result == -1) {
        ramfs_create("demo");
        result = ramfs_write("demo", "version; sysinfo; programs; runprog hello Ahmad; runprog clock; ps");
    }

    if (result == 1) {
        kprint("Demo script saved as RAM file: demo\n");
        kprint("Run it with: RUNFILE demo\n");
    } else {
        kprint("Could not create demo script.\n");
    }
}

static void command_script(char *args) {
    char subcommand[32];
    char *script_args;

    script_args = read_word(args, subcommand, 32);
    script_args = skip_spaces_local(script_args);

    if (subcommand[0] == '\0' ||
        strcmp_ignore_case(subcommand, "HELP") == 0) {
        command_script_help();
        return;
    }

    if (strcmp_ignore_case(subcommand, "STATUS") == 0) {
        command_script_status();
        return;
    }

    if (strcmp_ignore_case(subcommand, "DEMO") == 0) {
        command_script_demo();
        return;
    }

    if (strcmp_ignore_case(subcommand, "RUN") == 0) {
        command_runfile(script_args);
        return;
    }

    command_runfile(args);
}


static void command_run(char *args) {
    char app[32];

    args = read_word(args, app, 32);
    args = skip_spaces_local(args);

    if (app[0] == '\0') {
        kprint("Usage: RUN CALC|NOTES|FILES|EDITOR FILE\n");
        return;
    }

    calculator_mode = 0;
    notes_mode = 0;
    files_mode = 0;
    editor_mode = 0;

    if (strcmp_ignore_case(app, "CALC") == 0 || strcmp_ignore_case(app, "CALCULATOR") == 0) {
        calculator_mode = 1;
        print_calculator_help();
    } else if (strcmp_ignore_case(app, "NOTES") == 0 || strcmp_ignore_case(app, "NOTE") == 0) {
        notes_mode = 1;
        print_notes_help();
    } else if (strcmp_ignore_case(app, "FILES") == 0 || strcmp_ignore_case(app, "FILE") == 0 || strcmp_ignore_case(app, "FS") == 0) {
        files_mode = 1;
        print_files_help();
    } else if (strcmp_ignore_case(app, "EDITOR") == 0 || strcmp_ignore_case(app, "EDIT") == 0) {
        start_editor(args);
    } else {
        kprint("Unknown app. Use APPS.\n");
    }
}

static void run_notes(char *input) {
    char command[32];
    char *args;

    input = skip_spaces_local(input);
    args = read_word(input, command, 32);
    args = skip_spaces_local(args);

    if (command[0] == '\0') {
        return;
    }

    if (strcmp_ignore_case(command, "EXIT") == 0 || strcmp_ignore_case(command, "QUIT") == 0) {
        notes_mode = 0;
        kprint("Leaving notes app.\n");
    } else if (strcmp_ignore_case(command, "HELP") == 0) {
        print_notes_help();
    } else if (strcmp_ignore_case(command, "LIST") == 0 || strcmp_ignore_case(command, "LS") == 0) {
        note_list();
    } else if (strcmp_ignore_case(command, "SAVE") == 0 || strcmp_ignore_case(command, "WRITE") == 0) {
        note_save(args);
    } else if (strcmp_ignore_case(command, "OPEN") == 0 || strcmp_ignore_case(command, "SHOW") == 0 || strcmp_ignore_case(command, "READ") == 0) {
        note_open(args);
    } else if (strcmp_ignore_case(command, "APPEND") == 0) {
        note_append(args);
    } else if (strcmp_ignore_case(command, "DELETE") == 0 || strcmp_ignore_case(command, "RM") == 0 || strcmp_ignore_case(command, "DEL") == 0) {
        note_delete(args);
    } else if (strcmp_ignore_case(command, "FS") == 0) {
        command_fs(args);
    } else if (strcmp_ignore_case(command, "CLEAR") == 0 || strcmp_ignore_case(command, "CLS") == 0) {
        clear_screen();
        kprint("Notes app mode");
    } else {
        kprint("Unknown notes command. Use HELP.\n");
    }
}

static void run_files(char *input) {
    char command[32];
    char *args;

    input = skip_spaces_local(input);
    args = read_word(input, command, 32);
    args = skip_spaces_local(args);

    if (command[0] == '\0') {
        return;
    }

    if (strcmp_ignore_case(command, "EXIT") == 0 || strcmp_ignore_case(command, "QUIT") == 0) {
        files_mode = 0;
        kprint("Leaving files app.\n");
    } else if (strcmp_ignore_case(command, "HELP") == 0) {
        print_files_help();
    } else if (strcmp_ignore_case(command, "LS") == 0 || strcmp_ignore_case(command, "DIR") == 0) {
        command_ls(args);
    } else if (strcmp_ignore_case(command, "CREATE") == 0 || strcmp_ignore_case(command, "TOUCH") == 0) {
        command_touch(args);
    } else if (strcmp_ignore_case(command, "WRITE") == 0 || strcmp_ignore_case(command, "SAVE") == 0) {
        command_write(args);
    } else if (strcmp_ignore_case(command, "APPEND") == 0) {
        command_append(args);
    } else if (strcmp_ignore_case(command, "READ") == 0 || strcmp_ignore_case(command, "CAT") == 0 || strcmp_ignore_case(command, "TYPE") == 0 || strcmp_ignore_case(command, "OPEN") == 0) {
        command_cat(args);
    } else if (strcmp_ignore_case(command, "EDIT") == 0 || strcmp_ignore_case(command, "EDITOR") == 0) {
        start_editor(args);
    } else if (strcmp_ignore_case(command, "RUNFILE") == 0 || strcmp_ignore_case(command, "SCRIPT") == 0) {
        command_runfile(args);
    } else if (strcmp_ignore_case(command, "DISKINFO") == 0) {
        command_diskinfo();
    } else if (strcmp_ignore_case(command, "FORMATFS") == 0) {
        command_formatfs();
    } else if (strcmp_ignore_case(command, "SAVEFS") == 0) {
        command_savefs();
    } else if (strcmp_ignore_case(command, "LOADFS") == 0) {
        command_loadfs();
    } else if (strcmp_ignore_case(command, "SIZE") == 0) {
        command_size(args);
    } else if (strcmp_ignore_case(command, "RENAME") == 0 || strcmp_ignore_case(command, "MV") == 0) {
        command_rename(args);
    } else if (strcmp_ignore_case(command, "COPY") == 0 || strcmp_ignore_case(command, "CP") == 0) {
        command_copy(args);
    } else if (strcmp_ignore_case(command, "EXISTS") == 0) {
        command_exists(args);
    } else if (strcmp_ignore_case(command, "CLEARFILE") == 0) {
        command_clearfile(args);
    } else if (strcmp_ignore_case(command, "DELETE") == 0 || strcmp_ignore_case(command, "RM") == 0 || strcmp_ignore_case(command, "DEL") == 0) {
        command_rm(args);
    } else if (strcmp_ignore_case(command, "FS") == 0 || strcmp_ignore_case(command, "INFO") == 0) {
        command_fs(args);
    } else if (strcmp_ignore_case(command, "CLEAR") == 0 || strcmp_ignore_case(command, "CLS") == 0) {
        if (args[0] != '\0') {
            command_clearfile(args);
        } else {
            clear_screen();
            kprint("Files app mode");
        }
    } else {
        kprint("Unknown files command. Use HELP.\n");
    }
}

static void run_editor(char *input) {
    char command[32];
    char *args;

    input = skip_spaces_local(input);

    if (input[0] == '.') {
        input++;
    }

    args = read_word(input, command, 32);
    args = skip_spaces_local(args);

    if (command[0] == '\0') {
        return;
    }

    if (strcmp_ignore_case(command, "EXIT") == 0 || strcmp_ignore_case(command, "QUIT") == 0) {
        editor_mode = 0;
        editor_filename[0] = '\0';
        kprint("Leaving editor app.\n");
    } else if (strcmp_ignore_case(command, "HELP") == 0) {
        print_editor_help();
    } else if (strcmp_ignore_case(command, "SHOW") == 0 || strcmp_ignore_case(command, "READ") == 0 || strcmp_ignore_case(command, "CAT") == 0) {
        editor_show();
    } else if (strcmp_ignore_case(command, "SAVE") == 0 || strcmp_ignore_case(command, "WRITE") == 0) {
        editor_save(args);
    } else if (strcmp_ignore_case(command, "ADD") == 0 || strcmp_ignore_case(command, "APPEND") == 0) {
        editor_add(args);
    } else if (strcmp_ignore_case(command, "CLEAR") == 0 || strcmp_ignore_case(command, "CLEARFILE") == 0) {
        editor_clear_file();
    } else if (strcmp_ignore_case(command, "INFO") == 0 || strcmp_ignore_case(command, "SIZE") == 0) {
        editor_info();
    } else if (strcmp_ignore_case(command, "CLS") == 0 || strcmp_ignore_case(command, "SCREEN") == 0) {
        clear_screen();
        kprint("Editor app mode");
    } else {
        kprint("Unknown editor command. Use .HELP.\n");
    }
}

static void run_calculator(char *input) {
    int a;
    int b;
    int result;
    char op;

    if (strcmp_ignore_case(input, "EXIT") == 0 || strcmp_ignore_case(input, "QUIT") == 0) {
        calculator_mode = 0;
        kprint("Leaving calculator app.\n");
        return;
    }

    if (strcmp_ignore_case(input, "HELP") == 0) {
        print_calculator_help();
        return;
    }

    if (strcmp_ignore_case(input, "ANS") == 0) {
        kprint("ANS = ");
        print_int(calc_last_answer);
        kprint("\n");
        return;
    }

    if (strcmp_ignore_case(input, "CLEAR") == 0 || strcmp_ignore_case(input, "CLS") == 0) {
        clear_screen();
        kprint("Calculator app mode");
        return;
    }

    if (input[0] == '\0') {
        return;
    }

    if (!parse_calculator_expression(input, &a, &op, &b)) {
        kprint("Invalid calculator expression. Example: 5 + 3\n");
        return;
    }

    if (op == '+') {
        result = a + b;
    } else if (op == '-') {
        result = a - b;
    } else if (op == '*') {
        result = a * b;
    } else if (op == '/') {
        if (b == 0) {
            kprint("Error: division by zero\n");
            return;
        }
        result = a / b;
    } else if (op == '%') {
        if (b == 0) {
            kprint("Error: modulo by zero\n");
            return;
        }
        result = a % b;
    } else {
        kprint("Unknown operator\n");
        return;
    }

    calc_last_answer = result;
    kprint("= ");
    print_int(result);
    kprint("\n");
}

void user_input(char *input) {
    char command[32];
    char *args;

    trim_right(input);
    input = skip_spaces_local(input);

    if (calculator_mode) {
        run_calculator(input);
        print_prompt();
        return;
    }

    if (notes_mode) {
        run_notes(input);
        print_prompt();
        return;
    }

    if (files_mode) {
        run_files(input);
        print_prompt();
        return;
    }

    if (editor_mode) {
        run_editor(input);
        print_prompt();
        return;
    }

    if (input[0] == '\0') {
        print_prompt();
        return;
    }

    args = read_word(input, command, 32);
    args = skip_spaces_local(args);

    if (strcmp_ignore_case(command, "END") == 0 || strcmp_ignore_case(command, "HALT") == 0) {
        halt_cpu();
    } else if (strcmp_ignore_case(command, "HELP") == 0) {
        print_help();
    } else if (strcmp_ignore_case(command, "APPS") == 0) {
        print_apps();

    } else if (strcmp_ignore_case(command, "PROGRAMS") == 0 ||
               strcmp_ignore_case(command, "PROGS") == 0) {
        program_list();

    } else if (strcmp_ignore_case(command, "PROGRAM") == 0) {
        char subcommand[32];
        char *program_args;

        program_args = read_word(args, subcommand, 32);
        program_args = skip_spaces_local(program_args);

        if (strcmp_ignore_case(subcommand, "HELP") == 0 ||
            subcommand[0] == '\0') {
            program_help();
        } else if (strcmp_ignore_case(subcommand, "COUNT") == 0) {
            kprint("Program count: ");
            print_int(program_count());
            kprint("\n");
        } else if (strcmp_ignore_case(subcommand, "STATUS") == 0) {
            program_status();
        } else if (strcmp_ignore_case(subcommand, "INFO") == 0) {
            program_info(program_args);
        } else {
            kprint("Unknown PROGRAM command. Use PROGRAM HELP.\n");
        }

    } else if (strcmp_ignore_case(command, "RUNPROG") == 0 ||
               strcmp_ignore_case(command, "EXEC") == 0) {
        char program_name[32];
        char *program_args;

        program_args = read_word(args, program_name, 32);
        program_args = skip_spaces_local(program_args);

        program_run(program_name, program_args);

    } else if (strcmp_ignore_case(command, "PS") == 0) {
        process_list();

    } else if (strcmp_ignore_case(command, "PROCESS") == 0 ||
               strcmp_ignore_case(command, "PROC") == 0) {
        char subcommand[32];
        char *process_args;

        process_args = read_word(args, subcommand, 32);
        process_args = skip_spaces_local(process_args);
        process_args = process_args;

        if (subcommand[0] == '\0' ||
            strcmp_ignore_case(subcommand, "HELP") == 0) {
            process_help();
        } else if (strcmp_ignore_case(subcommand, "LIST") == 0 ||
                   strcmp_ignore_case(subcommand, "PS") == 0) {
            process_list();
        } else if (strcmp_ignore_case(subcommand, "STATUS") == 0) {
            process_status();
        } else if (strcmp_ignore_case(subcommand, "COUNT") == 0) {
            kprint("Process records: ");
            print_int(process_count());
            kprint("\n");
        } else if (strcmp_ignore_case(subcommand, "CLEAR") == 0 ||
                   strcmp_ignore_case(subcommand, "RESET") == 0) {
            process_clear();
        } else {
            kprint("Unknown PROCESS command. Use PROCESS HELP.\n");
        }

    } else if (strcmp_ignore_case(command, "RUN") == 0) {
        command_run(args);
    } else if (strcmp_ignore_case(command, "CLEAR") == 0 || strcmp_ignore_case(command, "CLS") == 0) {
        clear_screen();
        kprint("Simple_OS");
    } else if (strcmp_ignore_case(command, "VERSION") == 0) {
        kprint("Simple_OS version ");
        kprint(OS_VERSION);
        kprint("\n");
    } else if (strcmp_ignore_case(command, "ABOUT") == 0) {
        kprint("Simple_OS / MyOS\n");
        kprint("A small 32-bit x86 hobby CLI operating system.\n");
        kprint("Boots in QEMU using NASM Assembly and freestanding C.\n");
    } else if (strcmp_ignore_case(command, "BANNER") == 0) {
        print_banner();
    } else if (strcmp_ignore_case(command, "SYSINFO") == 0) {
        command_sysinfo();
    } else if (strcmp_ignore_case(command, "SYSCALLS") == 0 || strcmp_ignore_case(command, "SYSTEMCALLS") == 0) {
        command_syscalls();
    } else if (strcmp_ignore_case(command, "SYSCALL") == 0) {
        command_syscall(args);
    } else if (strcmp_ignore_case(command, "TICKS") == 0) {
        print_uint(get_timer_ticks());
        kprint("\n");
    } else if (strcmp_ignore_case(command, "UPTIME") == 0) {
        print_uint(get_timer_seconds());
        kprint(" seconds\n");
    } else if (strcmp_ignore_case(command, "MEMORY") == 0) {
        command_memory();
    } else if (strcmp_ignore_case(command, "MEMMAP") == 0) {
        command_memmap();
    } else if (strcmp_ignore_case(command, "PAGING") == 0) {
        command_paging();
    } else if (strcmp_ignore_case(command, "PAGE") == 0) {
        command_page(args);
    } else if (strcmp_ignore_case(command, "HEAP") == 0) {
        command_heap(args);
    } else if (strcmp_ignore_case(command, "ALLOC") == 0) {
        command_alloc(args);
    } else if (strcmp_ignore_case(command, "FREE") == 0) {
        command_free(args);
    } else if (strcmp_ignore_case(command, "DISKINFO") == 0) {
        command_diskinfo();
    } else if (strcmp_ignore_case(command, "FORMATFS") == 0) {
        command_formatfs();
    } else if (strcmp_ignore_case(command, "SAVEFS") == 0) {
        command_savefs();
    } else if (strcmp_ignore_case(command, "LOADFS") == 0) {
        command_loadfs();
    } else if (strcmp_ignore_case(command, "FS") == 0) {
        command_fs(args);
    } else if (strcmp_ignore_case(command, "LS") == 0 || strcmp_ignore_case(command, "DIR") == 0) {
        command_ls(args);
    } else if (strcmp_ignore_case(command, "TOUCH") == 0) {
        command_touch(args);
    } else if (strcmp_ignore_case(command, "WRITE") == 0) {
        command_write(args);
    } else if (strcmp_ignore_case(command, "APPEND") == 0) {
        command_append(args);
    } else if (strcmp_ignore_case(command, "CAT") == 0 || strcmp_ignore_case(command, "TYPE") == 0) {
        command_cat(args);
    } else if (strcmp_ignore_case(command, "EDIT") == 0 || strcmp_ignore_case(command, "EDITOR") == 0) {
        start_editor(args);
    } else if (strcmp_ignore_case(command, "RUNFILE") == 0) {
        command_runfile(args);
    } else if (strcmp_ignore_case(command, "SCRIPT") == 0) {
        command_script(args);
    } else if (strcmp_ignore_case(command, "MKFS") == 0) {
        command_formatfs();
    } else if (strcmp_ignore_case(command, "SIZE") == 0) {
        command_size(args);
    } else if (strcmp_ignore_case(command, "RENAME") == 0 || strcmp_ignore_case(command, "MV") == 0) {
        command_rename(args);
    } else if (strcmp_ignore_case(command, "COPY") == 0 || strcmp_ignore_case(command, "CP") == 0) {
        command_copy(args);
    } else if (strcmp_ignore_case(command, "EXISTS") == 0) {
        command_exists(args);
    } else if (strcmp_ignore_case(command, "CLEARFILE") == 0) {
        command_clearfile(args);
    } else if (strcmp_ignore_case(command, "RM") == 0 || strcmp_ignore_case(command, "DEL") == 0) {
        command_rm(args);
    } else if (strcmp_ignore_case(command, "NOTE") == 0) {
        command_note(args);
    } else if (strcmp_ignore_case(command, "COLOR") == 0) {
        command_color(args);
    } else if (strcmp_ignore_case(command, "CALC") == 0 || strcmp_ignore_case(command, "CALCULATOR") == 0) {
        calculator_mode = 1;
        print_calculator_help();
    } else if (strcmp_ignore_case(command, "NOTES") == 0) {
        notes_mode = 1;
        print_notes_help();
    } else if (strcmp_ignore_case(command, "FILES") == 0 || strcmp_ignore_case(command, "FILE") == 0) {
        files_mode = 1;
        print_files_help();
    } else if (strcmp_ignore_case(command, "ECHO") == 0) {
        command_echo(args);
    } else if (strcmp_ignore_case(command, "UPPER") == 0) {
        command_upper(args);
    } else if (strcmp_ignore_case(command, "LOWER") == 0) {
        command_lower(args);
    } else if (strcmp_ignore_case(command, "REVERSE") == 0) {
        command_reverse(args);
    } else if (strcmp_ignore_case(command, "LEN") == 0) {
        command_len(args);
    } else if (strcmp_ignore_case(command, "ASCII") == 0) {
        command_ascii(args);
    } else if (strcmp_ignore_case(command, "REPEAT") == 0) {
        command_repeat(args);
    } else if (strcmp_ignore_case(command, "COUNT") == 0) {
        command_count(args);
    } else if (strcmp_ignore_case(command, "RAND") == 0) {
        command_rand();
    } else if (strcmp_ignore_case(command, "ADD") == 0 || strcmp_ignore_case(command, "SUB") == 0 || strcmp_ignore_case(command, "MUL") == 0 || strcmp_ignore_case(command, "DIV") == 0 || strcmp_ignore_case(command, "MOD") == 0) {
        command_math(command, args);
    } else if (strcmp_ignore_case(command, "REBOOT") == 0) {
        reboot();
    } else {
        kprint(command);
        kprint(": command not found\n");
    }

    print_prompt();
}

void main() {
    char input[INPUT_SIZE];

    isr_install();

    init_timer(50);
    init_keyboard();

    irq_install();

    init_syscalls();
    init_heap();
    ramfs_init();
    init_paging();
    disk_init();
    simplefs_init();
    init_processes();
    init_programs();

    clear_screen();
    print_banner();

    kprint("Keyboard initialized.\n");
    kprint("Heap v2 initialized.\n");
    kprint("RAM filesystem initialized.\n");
    kprint("Paging initialized.\n");
    kprint("Disk driver initialized.\n");
    kprint("SimpleFS initialized.\n");
    kprint("Safe syscall dispatcher initialized.\n");
    kprint("App manager initialized.\n");
    kprint("Editor app initialized.\n");
    kprint("Script runner v2 initialized.\n");
    kprint("Program manager v2 initialized.\n");
    kprint("Process manager v1 initialized.\n");
    kprint("Type HELP to see available commands.");
    print_prompt();

    while (1) {
        if (keyboard_get_line(input, INPUT_SIZE)) {
            user_input(input);
        }

        asm volatile("hlt");
    }
}
