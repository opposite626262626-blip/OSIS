/*
 * OSIS Kernel - Basic Types
 */
#ifndef OSIS_TYPES_H
#define OSIS_TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint64_t           size_t;
typedef int64_t            ssize_t;
typedef uint64_t           uintptr_t;

#define NULL ((void *)0)
#define TRUE  1
#define FALSE 0
typedef int bool;

/* Boot info from UEFI bootloader */
typedef struct {
    uint64_t framebuffer_base;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t pixel_format; /* 0=RGBX, 1=BGRX */
    uint64_t kernel_base;
    uint64_t kernel_size;
    uint64_t memory_map_addr;
    uint64_t memory_map_size;
    uint64_t memory_map_desc_size;
    uint64_t font_base;
    uint64_t font_size;
} __attribute__((packed)) BootInfo;

/* Color definitions */
#define COLOR_BLACK       0x00000000
#define COLOR_WHITE       0x00FFFFFF
#define COLOR_BLUE        0x003070C0
#define COLOR_DARK_BLUE   0x001E3A5F
#define COLOR_LIGHT_BLUE  0x004A90D9
#define COLOR_GREEN       0x0020A020
#define COLOR_DARK_GREEN  0x00106010
#define COLOR_RED         0x00C03030
#define COLOR_YELLOW      0x00E0E020
#define COLOR_GRAY        0x00808080
#define COLOR_LIGHT_GRAY  0x00C0C0C0
#define COLOR_DARK_GRAY   0x00404040
#define COLOR_SKY_BLUE    0x0087CEEB
#define COLOR_GROUND      0x00567D46
#define COLOR_TASKBAR     0x002D2D30
#define COLOR_START_BTN   0x003070C0
#define COLOR_WINDOW_TITLE 0x001E3A5F
#define COLOR_WINDOW_BG   0x00F0F0F0
#define COLOR_GSOD_GREEN  0x00107010

#endif /* OSIS_TYPES_H */
