/*
 * OSIS Kernel - I/O Port Access
 */
#ifndef OSIS_PORT_H
#define OSIS_PORT_H

#include "../types.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* Simple delay using port I/O */
static inline void delay(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 1000; i++) {
        io_wait();
    }
}

/* PS/2 Keyboard */
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_CMD_PORT     0x64

static inline uint8_t kb_read_scancode(void) {
    while (!(inb(KB_STATUS_PORT) & 1));
    return inb(KB_DATA_PORT);
}

static inline int kb_has_key(void) {
    return inb(KB_STATUS_PORT) & 1;
}

/* PC Speaker for error beep */
static inline void speaker_on(uint32_t freq) {
    uint32_t div = 1193180 / freq;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 3);
}

static inline void speaker_off(void) {
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp & 0xFC);
}

#endif /* OSIS_PORT_H */
