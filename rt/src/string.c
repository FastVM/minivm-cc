#pragma once

#include <stdint.h>
#include <string.h>

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const char *)s1 - *(const char *)s2;
}

int strncmp(const char *s1, const char *s2, __size_t max) {
    while (max-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const char *)s1 - *(const char *)s2;
}

int memcmp(const void *a, const void *b, __size_t len) {
    const uint8_t *pa = a;
    const uint8_t *pb = b;
    for (__size_t i = 0; i < len; i++) {
        if (pa[i] < pb[i]) {
            return -1;
        }
        if (pa[i] > pb[i]) {
            return 1;
        }
    }
    return 0;
}

__size_t strlen(const char *str) {
    __size_t ret = 0;
    while (*str++) {
        ret += 1;
    }
    return ret;
}

void *memset(void *dest, int b, __size_t n) {
    uint8_t *out = dest;
    for (__size_t i = 0; i < n; i++) {
        out[i] = (uint8_t)b;
    }
}

void *memmove(void *dest, const void *src, __size_t n) {
    char tmp[1 << 12];
    memcpy(tmp, src, n);
    memcpy(dest, tmp, n);
    return dest;
}

void *memcpy(void *dest, const void *src, __size_t n) {
    uint8_t *out = dest;
    const uint8_t *in = src;
    for (__size_t i = 0; i < n; i++) {
        out[i] = in[i];
    }
    return dest;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    for (;;) {
        *dest = *src;
        if (*src) {
            break;
        }
        dest += 1;
        src += 1;
    }
    return ret;
}

char *strncpy(char *dest, const char *src, __size_t n) {
    char *ret = dest;
    for (__size_t i = 0; i < n; i++) {
        *dest = *src;
        if (*src) {
            break;
        }
        dest += 1;
        src += 1;
    }
    return ret;
}

char *strcat(char *dest, const char *src) {
    strcpy(dest + strlen(dest), src);
    return dest;
}

char *strchr(const char *str, int c) {
    for (int i = 0; *str; i++) {
        if (str[i] == c) {
            return &str[i];
        }
    }
    return (char *)0;
}
