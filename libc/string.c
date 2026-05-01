#include "string.h"

void int_to_ascii(int n, char str[]) {
    int i;
    int sign;

    if ((sign = n) < 0) {
        n = -n;
    }

    i = 0;

    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) {
        str[i++] = '-';
    }

    str[i] = '\0';

    reverse(str);
}

void reverse(char s[]) {
    int c;
    int i;
    int j;

    for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

int strlen(char s[]) {
    int i = 0;

    while (s[i] != '\0') {
        ++i;
    }

    return i;
}

void append(char s[], char n) {
    int len = strlen(s);

    s[len] = n;
    s[len + 1] = '\0';
}

void backspace(char s[]) {
    int len = strlen(s);

    if (len > 0) {
        s[len - 1] = '\0';
    }
}

int strcmp(char s1[], char s2[]) {
    int i;

    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            return 0;
        }
    }

    return s1[i] - s2[i];
}

char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }

    return c;
}

char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }

    return c;
}

int is_space(char c) {
    return c == ' ' || c == '\t';
}

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_alpha(char c) {
    if (c >= 'a' && c <= 'z') {
        return 1;
    }

    if (c >= 'A' && c <= 'Z') {
        return 1;
    }

    return 0;
}

int strcmp_ignore_case(char s1[], char s2[]) {
    int i = 0;

    while (s1[i] != '\0' && s2[i] != '\0') {
        if (to_upper(s1[i]) != to_upper(s2[i])) {
            return to_upper(s1[i]) - to_upper(s2[i]);
        }

        i++;
    }

    return to_upper(s1[i]) - to_upper(s2[i]);
}

int starts_with_ignore_case(char s[], char prefix[]) {
    int i = 0;

    while (prefix[i] != '\0') {
        if (to_upper(s[i]) != to_upper(prefix[i])) {
            return 0;
        }

        i++;
    }

    return 1;
}

void strcpy(char dest[], char src[]) {
    int i = 0;

    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

void strcat(char dest[], char src[]) {
    int i = strlen(dest);
    int j = 0;

    while (src[j] != '\0') {
        dest[i] = src[j];
        i++;
        j++;
    }

    dest[i] = '\0';
}

void str_to_upper(char s[]) {
    int i = 0;

    while (s[i] != '\0') {
        s[i] = to_upper(s[i]);
        i++;
    }
}

void str_to_lower(char s[]) {
    int i = 0;

    while (s[i] != '\0') {
        s[i] = to_lower(s[i]);
        i++;
    }
}