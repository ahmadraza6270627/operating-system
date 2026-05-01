#include "screen.h"
#include "../cpu/ports.h"
#include "../libc/mem.h"

static u8 current_color = WHITE_ON_BLACK;

int get_cursor_offset() {
    int offset;

    port_byte_out(REG_SCREEN_CTRL, 14);
    offset = port_byte_in(REG_SCREEN_DATA) << 8;

    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);

    return offset * 2;
}

void set_cursor_offset(int offset) {
    offset /= 2;

    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));

    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xFF));
}

int get_offset(int col, int row) {
    return 2 * (row * MAX_COLS + col);
}

int get_offset_row(int offset) {
    return offset / (2 * MAX_COLS);
}

int get_offset_col(int offset) {
    return (offset - (get_offset_row(offset) * 2 * MAX_COLS)) / 2;
}

int handle_scrolling(int cursor_offset) {
    int i;
    char *last_line;

    if (cursor_offset < MAX_ROWS * MAX_COLS * 2) {
        return cursor_offset;
    }

    for (i = 1; i < MAX_ROWS; i++) {
        memory_copy(
            (char *)(VIDEO_ADDRESS + get_offset(0, i)),
            (char *)(VIDEO_ADDRESS + get_offset(0, i - 1)),
            MAX_COLS * 2
        );
    }

    last_line = (char *)(VIDEO_ADDRESS + get_offset(0, MAX_ROWS - 1));

    for (i = 0; i < MAX_COLS * 2; i += 2) {
        last_line[i] = ' ';
        last_line[i + 1] = current_color;
    }

    cursor_offset -= 2 * MAX_COLS;

    return cursor_offset;
}

void set_text_color(u8 color) {
    current_color = color;
}

u8 get_text_color() {
    return current_color;
}

void clear_screen() {
    int screen_size = MAX_COLS * MAX_ROWS;
    int i;
    char *screen = (char *)VIDEO_ADDRESS;

    for (i = 0; i < screen_size; i++) {
        screen[i * 2] = ' ';
        screen[i * 2 + 1] = current_color;
    }

    set_cursor_offset(get_offset(0, 0));
}

void print_char_at(char character, int col, int row, char attribute_byte) {
    unsigned char *vidmem = (unsigned char *)VIDEO_ADDRESS;
    int offset;

    if (!attribute_byte) {
        attribute_byte = current_color;
    }

    if (col >= 0 && row >= 0) {
        offset = get_offset(col, row);
    } else {
        offset = get_cursor_offset();
    }

    if (character == '\n') {
        row = get_offset_row(offset);
        offset = get_offset(0, row + 1);
    } else if (character == '\r') {
        row = get_offset_row(offset);
        offset = get_offset(0, row);
    } else {
        vidmem[offset] = character;
        vidmem[offset + 1] = attribute_byte;
        offset += 2;
    }

    offset = handle_scrolling(offset);
    set_cursor_offset(offset);
}

void kprint_at(char *message, int col, int row) {
    if (col >= 0 && row >= 0) {
        set_cursor_offset(get_offset(col, row));
    }

    while (*message != 0) {
        print_char_at(*message, -1, -1, current_color);
        message++;
    }
}

void kprint(char *message) {
    kprint_at(message, -1, -1);
}

void kprint_colored(char *message, u8 color) {
    u8 old_color = current_color;

    set_text_color(color);
    kprint(message);
    set_text_color(old_color);
}

void kprint_backspace() {
    unsigned char *vidmem = (unsigned char *)VIDEO_ADDRESS;
    int offset = get_cursor_offset();

    if (offset <= 0) {
        return;
    }

    offset -= 2;

    vidmem[offset] = ' ';
    vidmem[offset + 1] = current_color;

    set_cursor_offset(offset);
}