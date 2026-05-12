/*
 * OSIS Kernel - Keyboard Driver (PS/2 Scancode Set 1)
 */
#ifndef OSIS_KEYBOARD_H
#define OSIS_KEYBOARD_H

#include "../types.h"
#include "port.h"

/* Scancode to ASCII mapping (US layout, set 1 - make codes only) */
static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,  ' ',0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0
};

static const char scancode_to_ascii_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,  ' ',0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0
};

#define KEY_ENTER     0x1C
#define KEY_BACKSPACE 0x0E
#define KEY_ESCAPE    0x01
#define KEY_TAB       0x0F
#define KEY_LSHIFT    0x2A
#define KEY_RSHIFT    0x36
#define KEY_LCTRL     0x1D
#define KEY_LALT      0x38
#define KEY_CAPSLOCK  0x3A
#define KEY_F1        0x3B
#define KEY_F2        0x3C
#define KEY_F3        0x3D
#define KEY_F4        0x3E
#define KEY_UP        0x48
#define KEY_DOWN      0x50
#define KEY_LEFT      0x4B
#define KEY_RIGHT     0x4D
#define KEY_SPACE     0x39

typedef struct {
    bool shift_held;
    bool ctrl_held;
    bool alt_held;
    bool caps_lock;
} KeyboardState;

static KeyboardState kb_state = {FALSE, FALSE, FALSE, FALSE};

static inline char kb_get_char(void) {
    while (1) {
        if (!kb_has_key()) continue;
        uint8_t sc = inb(KB_DATA_PORT);

        /* Handle key releases (bit 7 set) */
        if (sc & 0x80) {
            uint8_t released = sc & 0x7F;
            if (released == KEY_LSHIFT || released == KEY_RSHIFT)
                kb_state.shift_held = FALSE;
            if (released == KEY_LCTRL)
                kb_state.ctrl_held = FALSE;
            if (released == KEY_LALT)
                kb_state.alt_held = FALSE;
            continue;
        }

        /* Handle key presses */
        if (sc == KEY_LSHIFT || sc == KEY_RSHIFT) {
            kb_state.shift_held = TRUE;
            continue;
        }
        if (sc == KEY_LCTRL) {
            kb_state.ctrl_held = TRUE;
            continue;
        }
        if (sc == KEY_LALT) {
            kb_state.alt_held = TRUE;
            continue;
        }
        if (sc == KEY_CAPSLOCK) {
            kb_state.caps_lock = !kb_state.caps_lock;
            continue;
        }

        char c;
        if (kb_state.shift_held)
            c = scancode_to_ascii_shift[sc];
        else
            c = scancode_to_ascii[sc];

        /* Apply caps lock for letters */
        if (kb_state.caps_lock && c >= 'a' && c <= 'z')
            c -= 32;
        else if (kb_state.caps_lock && c >= 'A' && c <= 'Z')
            c += 32;

        if (c != 0) return c;
    }
}

/* Non-blocking key check - returns 0 if no key */
static inline char kb_try_get_char(void) {
    if (!kb_has_key()) return 0;
    uint8_t sc = inb(KB_DATA_PORT);

    if (sc & 0x80) {
        uint8_t released = sc & 0x7F;
        if (released == KEY_LSHIFT || released == KEY_RSHIFT)
            kb_state.shift_held = FALSE;
        if (released == KEY_LCTRL)
            kb_state.ctrl_held = FALSE;
        return 0;
    }

    if (sc == KEY_LSHIFT || sc == KEY_RSHIFT) {
        kb_state.shift_held = TRUE;
        return 0;
    }
    if (sc == KEY_LCTRL) {
        kb_state.ctrl_held = TRUE;
        return 0;
    }
    if (sc == KEY_LALT) {
        kb_state.alt_held = TRUE;
        return 0;
    }

    char c = kb_state.shift_held ? scancode_to_ascii_shift[sc] : scancode_to_ascii[sc];
    if (kb_state.caps_lock && c >= 'a' && c <= 'z') c -= 32;
    return c;
}

/* Get raw scancode (non-blocking) */
static inline uint8_t kb_try_get_scancode(void) {
    if (!kb_has_key()) return 0;
    return inb(KB_DATA_PORT);
}

/* Read a line of text into buffer, with visual echo on screen */
typedef void (*echo_callback_t)(char c, void *ctx);

static inline int kb_read_line(char *buf, int max_len, echo_callback_t echo_fn, void *ctx) {
    int pos = 0;
    while (pos < max_len - 1) {
        char c = kb_get_char();
        if (c == '\n') {
            buf[pos] = 0;
            if (echo_fn) echo_fn('\n', ctx);
            return pos;
        }
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                if (echo_fn) echo_fn('\b', ctx);
            }
            continue;
        }
        buf[pos++] = c;
        if (echo_fn) echo_fn(c, ctx);
    }
    buf[pos] = 0;
    return pos;
}

#endif /* OSIS_KEYBOARD_H */
