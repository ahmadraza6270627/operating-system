#include "keyboard.h"
#include "screen.h"
#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "../libc/string.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define CAPS_LOCK 0x3A

#define LSHIFT 0x2A
#define RSHIFT 0x36
#define LSHIFT_RELEASE 0xAA
#define RSHIFT_RELEASE 0xB6

#define EXTENDED_SCANCODE 0xE0
#define ARROW_UP 0x48
#define ARROW_DOWN 0x50
#define ARROW_LEFT 0x4B
#define ARROW_RIGHT 0x4D
#define DELETE_KEY 0x53
#define HOME_KEY 0x47
#define END_KEY 0x4F

#define SC_MAX 57
#define KEY_BUFFER_SIZE 256
#define HISTORY_SIZE 8

static char key_buffer[KEY_BUFFER_SIZE];
static char line_buffer[KEY_BUFFER_SIZE];

static char command_history[HISTORY_SIZE][KEY_BUFFER_SIZE];
static int history_count = 0;
static int history_index = 0;

static volatile int line_ready = 0;

static int key_len = 0;
static int cursor_index = 0;
static int input_start_offset = 0;
static int editing_active = 0;

static int shift_pressed = 0;
static int caps_lock_on = 0;
static int extended_key = 0;

const char sc_ascii[] = {
    '?', '?', '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '?', '?',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '?', '?', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', '?', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '?', '?',
    '?', ' '
};

const char sc_ascii_shift[] = {
    '?', '?', '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '?', '?',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '?', '?', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', '?', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '?', '?',
    '?', ' '
};

static int is_letter_key(char c) {
    if (c >= 'a' && c <= 'z') {
        return 1;
    }

    if (c >= 'A' && c <= 'Z') {
        return 1;
    }

    return 0;
}

static char key_to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }

    return c;
}

static char key_to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }

    return c;
}

static char apply_caps(char c) {
    if (!is_letter_key(c)) {
        return c;
    }

    if (caps_lock_on && !shift_pressed) {
        return key_to_upper(c);
    }

    if (caps_lock_on && shift_pressed) {
        return key_to_lower(c);
    }

    return c;
}

static void copy_string_limited(char dest[], char src[], int max_len) {
    int i = 0;

    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

static void ensure_editing_started() {
    if (!editing_active) {
        input_start_offset = get_cursor_offset();
        editing_active = 1;
    }
}

static int get_line_capacity() {
    int col;
    int capacity;

    ensure_editing_started();

    col = get_offset_col(input_start_offset);
    capacity = MAX_COLS - col - 1;

    if (capacity > KEY_BUFFER_SIZE - 1) {
        capacity = KEY_BUFFER_SIZE - 1;
    }

    if (capacity < 0) {
        capacity = 0;
    }

    return capacity;
}

static void reset_key_buffer_state() {
    key_buffer[0] = '\0';
    key_len = 0;
    cursor_index = 0;
    input_start_offset = 0;
    editing_active = 0;
}

static void redraw_input_area(int old_len) {
    int i;
    char str[2];

    ensure_editing_started();

    set_cursor_offset(input_start_offset);

    str[1] = '\0';

    for (i = 0; i < key_len; i++) {
        str[0] = key_buffer[i];
        kprint(str);
    }

    for (i = key_len; i < old_len; i++) {
        kprint(" ");
    }

    set_cursor_offset(input_start_offset + cursor_index * 2);
}

static void set_current_input(char text[]) {
    int i = 0;
    int old_len = key_len;
    int capacity;

    ensure_editing_started();

    capacity = get_line_capacity();

    while (text[i] != '\0' && i < capacity) {
        key_buffer[i] = text[i];
        i++;
    }

    key_buffer[i] = '\0';
    key_len = i;
    cursor_index = key_len;

    redraw_input_area(old_len);
}

static void move_cursor_left() {
    ensure_editing_started();

    if (cursor_index > 0) {
        cursor_index--;
        set_cursor_offset(input_start_offset + cursor_index * 2);
    }
}

static void move_cursor_right() {
    ensure_editing_started();

    if (cursor_index < key_len) {
        cursor_index++;
        set_cursor_offset(input_start_offset + cursor_index * 2);
    }
}

static void move_cursor_home() {
    ensure_editing_started();

    cursor_index = 0;
    set_cursor_offset(input_start_offset);
}

static void move_cursor_end() {
    ensure_editing_started();

    cursor_index = key_len;
    set_cursor_offset(input_start_offset + cursor_index * 2);
}

static void insert_character(char c) {
    int i;
    int old_len = key_len;
    int capacity;

    ensure_editing_started();

    capacity = get_line_capacity();

    if (key_len >= capacity) {
        return;
    }

    for (i = key_len; i > cursor_index; i--) {
        key_buffer[i] = key_buffer[i - 1];
    }

    key_buffer[cursor_index] = c;
    key_len++;
    cursor_index++;
    key_buffer[key_len] = '\0';

    redraw_input_area(old_len);
}

static void delete_before_cursor() {
    int i;
    int old_len = key_len;

    ensure_editing_started();

    if (cursor_index <= 0) {
        return;
    }

    for (i = cursor_index - 1; i < key_len; i++) {
        key_buffer[i] = key_buffer[i + 1];
    }

    key_len--;
    cursor_index--;

    redraw_input_area(old_len);
}

static void delete_at_cursor() {
    int i;
    int old_len = key_len;

    ensure_editing_started();

    if (cursor_index >= key_len) {
        return;
    }

    for (i = cursor_index; i < key_len; i++) {
        key_buffer[i] = key_buffer[i + 1];
    }

    key_len--;

    redraw_input_area(old_len);
}

static void save_to_history(char command[]) {
    int i;

    if (command[0] == '\0') {
        return;
    }

    if (history_count > 0) {
        if (strcmp(command_history[history_count - 1], command) == 0) {
            history_index = history_count;
            return;
        }
    }

    if (history_count < HISTORY_SIZE) {
        copy_string_limited(command_history[history_count], command, KEY_BUFFER_SIZE);
        history_count++;
    } else {
        for (i = 1; i < HISTORY_SIZE; i++) {
            copy_string_limited(command_history[i - 1], command_history[i], KEY_BUFFER_SIZE);
        }

        copy_string_limited(command_history[HISTORY_SIZE - 1], command, KEY_BUFFER_SIZE);
    }

    history_index = history_count;
}

static void history_up() {
    if (history_count == 0) {
        return;
    }

    if (history_index > 0) {
        history_index--;
    }

    set_current_input(command_history[history_index]);
}

static void history_down() {
    if (history_count == 0) {
        return;
    }

    if (history_index < history_count - 1) {
        history_index++;
        set_current_input(command_history[history_index]);
    } else {
        history_index = history_count;
        set_current_input("");
    }
}

static void save_line() {
    int i = 0;

    ensure_editing_started();

    if (line_ready) {
        return;
    }

    while (key_buffer[i] != '\0' && i < KEY_BUFFER_SIZE - 1) {
        line_buffer[i] = key_buffer[i];
        i++;
    }

    line_buffer[i] = '\0';

    save_to_history(key_buffer);

    line_ready = 1;

    reset_key_buffer_state();
}

static void keyboard_callback(registers_t *regs) {
    u8 scancode = port_byte_in(0x60);

    if (scancode == EXTENDED_SCANCODE) {
        extended_key = 1;
        return;
    }

    if (extended_key) {
        extended_key = 0;

        if (scancode & 0x80) {
            return;
        }

        if (line_ready) {
            return;
        }

        if (scancode == ARROW_UP) {
            history_up();
            return;
        }

        if (scancode == ARROW_DOWN) {
            history_down();
            return;
        }

        if (scancode == ARROW_LEFT) {
            move_cursor_left();
            return;
        }

        if (scancode == ARROW_RIGHT) {
            move_cursor_right();
            return;
        }

        if (scancode == DELETE_KEY) {
            delete_at_cursor();
            return;
        }

        if (scancode == HOME_KEY) {
            move_cursor_home();
            return;
        }

        if (scancode == END_KEY) {
            move_cursor_end();
            return;
        }

        return;
    }

    if (scancode == LSHIFT || scancode == RSHIFT) {
        shift_pressed = 1;
        return;
    }

    if (scancode == LSHIFT_RELEASE || scancode == RSHIFT_RELEASE) {
        shift_pressed = 0;
        return;
    }

    if (scancode == CAPS_LOCK) {
        caps_lock_on = !caps_lock_on;
        return;
    }

    if (scancode & 0x80) {
        return;
    }

    if (line_ready) {
        return;
    }

    if (scancode > SC_MAX) {
        return;
    }

    if (scancode == BACKSPACE) {
        delete_before_cursor();
        return;
    }

    if (scancode == ENTER) {
        kprint("\n");
        save_line();
        return;
    }

    {
        char letter;

        if (shift_pressed) {
            letter = sc_ascii_shift[(int)scancode];
        } else {
            letter = sc_ascii[(int)scancode];
            letter = apply_caps(letter);
        }

        if (letter == '?') {
            return;
        }

        insert_character(letter);
    }
}

void init_keyboard() {
    reset_key_buffer_state();

    line_buffer[0] = '\0';
    line_ready = 0;

    history_count = 0;
    history_index = 0;

    shift_pressed = 0;
    caps_lock_on = 0;
    extended_key = 0;

    add_interrupt_handler(IRQ1, keyboard_callback);
}

int keyboard_get_line(char *buffer, int max_len) {
    int i = 0;

    if (!line_ready) {
        return 0;
    }

    __asm__ __volatile__("cli");

    while (line_buffer[i] != '\0' && i < max_len - 1) {
        buffer[i] = line_buffer[i];
        i++;
    }

    buffer[i] = '\0';

    line_buffer[0] = '\0';
    line_ready = 0;

    __asm__ __volatile__("sti");

    return 1;
}