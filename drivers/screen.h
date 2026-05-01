#ifndef SCREEN_H
#define SCREEN_H

#include "../cpu/types.h"

#define VIDEO_ADDRESS 0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80

#define REG_SCREEN_CTRL 0x3D4
#define REG_SCREEN_DATA 0x3D5

#define BLACK_ON_BLACK 0x00
#define BLUE_ON_BLACK 0x01
#define GREEN_ON_BLACK 0x02
#define CYAN_ON_BLACK 0x03
#define RED_ON_BLACK 0x04
#define MAGENTA_ON_BLACK 0x05
#define BROWN_ON_BLACK 0x06
#define LIGHT_GREY_ON_BLACK 0x07
#define DARK_GREY_ON_BLACK 0x08
#define LIGHT_BLUE_ON_BLACK 0x09
#define LIGHT_GREEN_ON_BLACK 0x0A
#define LIGHT_CYAN_ON_BLACK 0x0B
#define LIGHT_RED_ON_BLACK 0x0C
#define LIGHT_MAGENTA_ON_BLACK 0x0D
#define YELLOW_ON_BLACK 0x0E
#define WHITE_ON_BLACK 0x0F

int get_cursor_offset();
void set_cursor_offset(int offset);
int get_offset(int col, int row);
int get_offset_row(int offset);
int get_offset_col(int offset);
int handle_scrolling(int cursor_offset);

void set_text_color(u8 color);
u8 get_text_color();

void clear_screen();
void print_char_at(char character, int col, int row, char attribute_byte);
void kprint_at(char *message, int col, int row);
void kprint(char *message);
void kprint_colored(char *message, u8 color);
void kprint_backspace();

#endif