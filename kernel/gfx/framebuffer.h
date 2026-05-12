/*
 * OSIS Kernel - Framebuffer Graphics
 */
#ifndef OSIS_FRAMEBUFFER_H
#define OSIS_FRAMEBUFFER_H

#include "../types.h"

void gfx_init(BootInfo *info);
void gfx_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, int thickness);
void gfx_draw_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color, int thickness);
void gfx_fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color);
void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color, int thickness);
void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, int scale);
void gfx_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t color, int scale);
void gfx_draw_string_centered(uint32_t y, const char *str, uint32_t color, int scale);
void gfx_clear(uint32_t color);
void gfx_draw_progress_bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t progress, uint32_t fg, uint32_t bg);
void gfx_scroll_up(uint32_t lines, uint32_t bg_color);

uint32_t gfx_width(void);
uint32_t gfx_height(void);
uint32_t gfx_make_color(uint8_t r, uint8_t g, uint8_t b);

/* Rounded rectangle */
void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint32_t radius, uint32_t color);

/* Gradient */
void gfx_fill_gradient_v(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t color_top, uint32_t color_bottom);

#endif
