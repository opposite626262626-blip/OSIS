/*
 * OSIS Kernel - Framebuffer Graphics Implementation
 */
#include "framebuffer.h"
#include "../string.h"
#include "font8x16.h"

static uint32_t *fb = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;  /* pixels per scanline */
static uint32_t fb_pixel_format = 0;

void gfx_init(BootInfo *info) {
    fb = (uint32_t *)(uintptr_t)info->framebuffer_base;
    fb_width = info->width;
    fb_height = info->height;
    fb_pitch = info->pixels_per_scanline;
    fb_pixel_format = info->pixel_format;
}

uint32_t gfx_width(void)  { return fb_width; }
uint32_t gfx_height(void) { return fb_height; }

uint32_t gfx_make_color(uint8_t r, uint8_t g, uint8_t b) {
    if (fb_pixel_format == 1) /* BGRX */
        return (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    else /* RGBX */
        return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

void gfx_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb || x >= fb_width || y >= fb_height) return;
    fb[y * fb_pitch + x] = color;
}

void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = y; j < y + h && j < fb_height; j++) {
        uint32_t start = j * fb_pitch + x;
        for (uint32_t i = 0; i < w && (x + i) < fb_width; i++) {
            fb[start + i] = color;
        }
    }
}

void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                    uint32_t color, int thickness) {
    for (int t = 0; t < thickness; t++) {
        /* Top */
        for (uint32_t i = x; i < x + w; i++) gfx_put_pixel(i, y + t, color);
        /* Bottom */
        for (uint32_t i = x; i < x + w; i++) gfx_put_pixel(i, y + h - 1 - t, color);
        /* Left */
        for (uint32_t j = y; j < y + h; j++) gfx_put_pixel(x + t, j, color);
        /* Right */
        for (uint32_t j = y; j < y + h; j++) gfx_put_pixel(x + w - 1 - t, j, color);
    }
}

void gfx_draw_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color, int thickness) {
    for (int t = 0; t < thickness; t++) {
        int32_t rr = r - t;
        if (rr <= 0) break;
        int32_t x = 0, y = rr;
        int32_t d = 3 - 2 * rr;
        while (x <= y) {
            gfx_put_pixel(cx + x, cy + y, color);
            gfx_put_pixel(cx - x, cy + y, color);
            gfx_put_pixel(cx + x, cy - y, color);
            gfx_put_pixel(cx - x, cy - y, color);
            gfx_put_pixel(cx + y, cy + x, color);
            gfx_put_pixel(cx - y, cy + x, color);
            gfx_put_pixel(cx + y, cy - x, color);
            gfx_put_pixel(cx - y, cy - x, color);
            if (d < 0) {
                d += 4 * x + 6;
            } else {
                d += 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}

void gfx_fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color) {
    for (int32_t y = -r; y <= r; y++) {
        for (int32_t x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                gfx_put_pixel(cx + x, cy + y, color);
            }
        }
    }
}

void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                    uint32_t color, int thickness) {
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx - dy;

    while (1) {
        for (int t = -thickness/2; t <= thickness/2; t++) {
            gfx_put_pixel(x0 + t, y0, color);
            gfx_put_pixel(x0, y0 + t, color);
        }
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, int scale) {
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;
    for (int row = 0; row < 16; row++) {
        uint8_t bits = builtin_font[idx][row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                if (scale <= 1) {
                    gfx_put_pixel(x + col, y + row, color);
                } else {
                    gfx_fill_rect(x + col * scale, y + row * scale,
                                  scale, scale, color);
                }
            }
        }
    }
}

void gfx_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t color, int scale) {
    int char_w = 8 * (scale > 0 ? scale : 1);
    while (*str) {
        gfx_draw_char(x, y, *str, color, scale);
        x += char_w;
        str++;
    }
}

void gfx_draw_string_centered(uint32_t y, const char *str, uint32_t color, int scale) {
    int char_w = 8 * (scale > 0 ? scale : 1);
    uint32_t len = strlen(str);
    uint32_t total_w = len * char_w;
    uint32_t x = (fb_width > total_w) ? (fb_width - total_w) / 2 : 0;
    gfx_draw_string(x, y, str, color, scale);
}

void gfx_clear(uint32_t color) {
    for (uint32_t j = 0; j < fb_height; j++) {
        uint32_t start = j * fb_pitch;
        for (uint32_t i = 0; i < fb_width; i++) {
            fb[start + i] = color;
        }
    }
}

void gfx_draw_progress_bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t progress, uint32_t fg, uint32_t bg) {
    /* Background */
    gfx_fill_rect(x, y, w, h, bg);
    /* Border */
    gfx_draw_rect(x, y, w, h, COLOR_WHITE, 1);
    /* Fill */
    uint32_t fill_w = (w - 4) * progress / 100;
    if (fill_w > 0) {
        gfx_fill_rect(x + 2, y + 2, fill_w, h - 4, fg);
    }
}

void gfx_scroll_up(uint32_t lines, uint32_t bg_color) {
    uint32_t pixel_lines = lines * 16;
    if (pixel_lines >= fb_height) {
        gfx_clear(bg_color);
        return;
    }
    for (uint32_t y = 0; y < fb_height - pixel_lines; y++) {
        memcpy(&fb[y * fb_pitch], &fb[(y + pixel_lines) * fb_pitch],
               fb_width * sizeof(uint32_t));
    }
    gfx_fill_rect(0, fb_height - pixel_lines, fb_width, pixel_lines, bg_color);
}

void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t radius, uint32_t color) {
    /* Main body */
    gfx_fill_rect(x + radius, y, w - 2 * radius, h, color);
    gfx_fill_rect(x, y + radius, radius, h - 2 * radius, color);
    gfx_fill_rect(x + w - radius, y + radius, radius, h - 2 * radius, color);

    /* Corners using filled circle quadrants */
    int32_t r = (int32_t)radius;
    for (int32_t cy = -r; cy <= 0; cy++) {
        for (int32_t cx = -r; cx <= 0; cx++) {
            if (cx * cx + cy * cy <= r * r) {
                /* Top-left */
                gfx_put_pixel(x + radius + cx, y + radius + cy, color);
                /* Top-right */
                gfx_put_pixel(x + w - radius - 1 - cx, y + radius + cy, color);
                /* Bottom-left */
                gfx_put_pixel(x + radius + cx, y + h - radius - 1 - cy, color);
                /* Bottom-right */
                gfx_put_pixel(x + w - radius - 1 - cx, y + h - radius - 1 - cy, color);
            }
        }
    }
}

void gfx_fill_gradient_v(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t color_top, uint32_t color_bottom) {
    uint8_t r1 = (color_top >> 16) & 0xFF;
    uint8_t g1 = (color_top >> 8) & 0xFF;
    uint8_t b1 = color_top & 0xFF;
    uint8_t r2 = (color_bottom >> 16) & 0xFF;
    uint8_t g2 = (color_bottom >> 8) & 0xFF;
    uint8_t b2 = color_bottom & 0xFF;

    for (uint32_t j = 0; j < h && (y + j) < fb_height; j++) {
        uint8_t r = r1 + (r2 - r1) * j / h;
        uint8_t g = g1 + (g2 - g1) * j / h;
        uint8_t b = b1 + (b2 - b1) * j / h;
        uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        uint32_t start = (y + j) * fb_pitch + x;
        for (uint32_t i = 0; i < w && (x + i) < fb_width; i++) {
            fb[start + i] = c;
        }
    }
}
