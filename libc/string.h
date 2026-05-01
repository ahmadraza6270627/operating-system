#ifndef STRING_H
#define STRING_H

void int_to_ascii(int n, char str[]);
void reverse(char s[]);
int strlen(char s[]);
void append(char s[], char n);
void backspace(char s[]);
int strcmp(char s1[], char s2[]);

char to_upper(char c);
char to_lower(char c);
int is_space(char c);
int is_digit(char c);
int is_alpha(char c);
int strcmp_ignore_case(char s1[], char s2[]);
int starts_with_ignore_case(char s[], char prefix[]);
void strcpy(char dest[], char src[]);
void strcat(char dest[], char src[]);
void str_to_upper(char s[]);
void str_to_lower(char s[]);

#endif