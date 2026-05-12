/*
 * OSIS Kernel - String utilities
 */
#ifndef OSIS_STRING_H
#define OSIS_STRING_H

#include "types.h"

static inline void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

static inline void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dest;
}

static inline size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static inline int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

static inline char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static inline void itoa(int val, char *buf, int base) {
    char tmp[32];
    int i = 0;
    int neg = 0;
    if (val < 0 && base == 10) { neg = 1; val = -val; }
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (val > 0) {
        int d = val % base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('A' + d - 10);
        val /= base;
    }
    int j = 0;
    if (neg) buf[j++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

static inline void ultoa(uint64_t val, char *buf) {
    char tmp[24];
    int i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

#endif /* OSIS_STRING_H */
