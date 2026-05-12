/*
 * OSIS - Operating System Interface Security
 * Main Kernel
 *
 * Handles:
 *   1. Setup flow (partition creation, file copy, user setup)
 *   2. Desktop environment (taskbar, start menu, windows)
 *   3. Security system (Green Screen of Death, error codes)
 *   4. System services (Interface.exe, UC, Command.exe, Security.exe)
 */

#include "types.h"
#include "string.h"
#include "gfx/framebuffer.h"
#include "io/port.h"
#include "io/keyboard.h"

/* ==================== SYSTEM STATE ==================== */

typedef enum {
    STATE_BOOT_LOGO,
    STATE_SETUP_PARTITION,
    STATE_SETUP_COPY_EXIT,
    STATE_SETUP_COPYING,
    STATE_SETUP_RESTART_PROMPT,
    STATE_SETUP_COMPUTER_NAME,
    STATE_SETUP_FONT_SELECT,
    STATE_SETUP_USER_CREATE,
    STATE_SETUP_INSTALLING,
    STATE_SETUP_RESTART2,
    STATE_SETUP_FINAL_INSTALL,
    STATE_LOGIN_SELECT,
    STATE_LOGIN_PASSWORD,
    STATE_DESKTOP,
    STATE_GSOD,  /* Green Screen of Death */
    STATE_SHUTDOWN
} SystemState;

typedef struct {
    char computer_name[64];
    char username[64];
    char password[64];
    bool has_password;
    int selected_font;
    bool drive_d;
    bool drive_e;
    bool drive_f;
    bool drive_r;
    uint32_t drive_s_size; /* in GB, min 35 */
    uint32_t drive_d_size;
    uint32_t drive_e_size;
    uint32_t drive_f_size;
    uint32_t drive_r_size;
    bool setup_completed;
    bool first_boot_done;
} SystemConfig;

static BootInfo *g_boot_info = NULL;
static SystemConfig g_config;
static SystemState g_state = STATE_BOOT_LOGO;

/* ==================== UI HELPERS ==================== */

/* Draw a button */
static void draw_button(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                         const char *label, uint32_t bg, uint32_t fg, bool selected) {
    gfx_fill_rounded_rect(x, y, w, h, 4, bg);
    if (selected) {
        gfx_draw_rect(x - 1, y - 1, w + 2, h + 2, COLOR_WHITE, 2);
    }
    uint32_t tw = strlen(label) * 8;
    uint32_t tx = x + (w - tw) / 2;
    uint32_t ty = y + (h - 16) / 2;
    gfx_draw_string(tx, ty, label, fg, 1);
}

/* Draw a text input field */
static void draw_input_field(uint32_t x, uint32_t y, uint32_t w, const char *text,
                              const char *placeholder, bool focused, bool is_password) {
    gfx_fill_rect(x, y, w, 28, COLOR_WHITE);
    gfx_draw_rect(x, y, w, 28, focused ? COLOR_LIGHT_BLUE : COLOR_GRAY, 1);

    const char *display = text;
    char masked[64];
    if (is_password && strlen(text) > 0) {
        size_t len = strlen(text);
        if (len > 63) len = 63;
        for (size_t i = 0; i < len; i++) masked[i] = '*';
        masked[len] = 0;
        display = masked;
    }

    if (strlen(text) > 0) {
        gfx_draw_string(x + 6, y + 6, display, COLOR_BLACK, 1);
        /* Cursor */
        if (focused) {
            uint32_t cx = x + 6 + strlen(display) * 8;
            gfx_fill_rect(cx, y + 4, 2, 20, COLOR_BLACK);
        }
    } else if (placeholder) {
        gfx_draw_string(x + 6, y + 6, placeholder, COLOR_GRAY, 1);
        if (focused) {
            gfx_fill_rect(x + 6, y + 4, 2, 20, COLOR_BLACK);
        }
    }
}

/* Draw a checkbox */
static void draw_checkbox(uint32_t x, uint32_t y, const char *label,
                           bool checked, bool enabled) {
    uint32_t box_color = enabled ? COLOR_WHITE : COLOR_DARK_GRAY;
    gfx_fill_rect(x, y, 16, 16, box_color);
    gfx_draw_rect(x, y, 16, 16, COLOR_WHITE, 1);
    if (checked) {
        /* Draw checkmark */
        gfx_draw_line(x + 3, y + 8, x + 6, y + 12, COLOR_LIGHT_BLUE, 2);
        gfx_draw_line(x + 6, y + 12, x + 13, y + 3, COLOR_LIGHT_BLUE, 2);
    }
    gfx_draw_string(x + 22, y + 1, label, COLOR_WHITE, 1);
}

/* Draw OSIS logo - matching reference: vertical line through both top and bottom */
static void draw_osis_logo(uint32_t cx, uint32_t cy, uint32_t size) {
    int32_t r_outer = size;
    int32_t r_inner = size * 2 / 5;
    int32_t line_extend = size / 4;

    /* Vertical line through entire logo (top to bottom, extends past circles) */
    gfx_draw_line(cx, cy - r_outer - line_extend, cx, cy + r_outer + line_extend,
                  COLOR_WHITE, 3);
    /* Outer circle (double ring effect) */
    gfx_draw_circle(cx, cy, r_outer, COLOR_WHITE, 3);
    gfx_draw_circle(cx, cy, r_outer - 4, COLOR_WHITE, 2);
    /* Inner circle (double ring effect) */
    gfx_draw_circle(cx, cy, r_inner, COLOR_WHITE, 2);
    gfx_draw_circle(cx, cy, r_inner - 3, COLOR_WHITE, 1);
}

/* ==================== BOOT LOGO SCREEN ==================== */

static void screen_boot_logo(void) {
    gfx_clear(COLOR_BLACK);

    uint32_t cx = gfx_width() / 2;
    uint32_t cy = gfx_height() / 2 - 60;

    /* Draw OSIS logo */
    draw_osis_logo(cx, cy, 50);

    /* "Setup file for you" text */
    gfx_draw_string_centered(cy + 90, "Setup file for you", COLOR_WHITE, 2);

    /* Spinning animation */
    const char *spinner[] = {"|", "/", "-", "\\"};
    for (int i = 0; i < 30; i++) {
        gfx_fill_rect(cx - 10, cy + 130, 20, 20, COLOR_BLACK);
        gfx_draw_string(cx - 4, cy + 130, spinner[i % 4], COLOR_WHITE, 1);
        delay(200);
    }

    g_state = STATE_SETUP_PARTITION;
}

/* ==================== SETUP: PARTITION SCREEN ==================== */

static void screen_setup_partition(void) {
    gfx_clear(COLOR_BLUE);

    /* Title bar */
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Drive Configuration", COLOR_WHITE, 1);

    /* Description */
    gfx_draw_string(40, 60, "Configure your drive partitions:", COLOR_WHITE, 1);
    gfx_draw_string(40, 80, "S drive is the system drive (min 35 GB, cannot be unchecked)", COLOR_WHITE, 1);
    gfx_draw_string(40, 96, "Other drives start at 4 GB and can be adjusted up", COLOR_WHITE, 1);

    /* Drive S - always checked, grayed out checkbox */
    g_config.drive_s_size = 35;
    g_config.drive_d_size = 4;
    g_config.drive_e_size = 4;
    g_config.drive_f_size = 4;
    g_config.drive_r_size = 4;

    /* Initial selections */
    g_config.drive_d = TRUE;
    g_config.drive_e = FALSE;
    g_config.drive_f = FALSE;
    g_config.drive_r = TRUE;

    int sel = 0; /* 0-3: D,E,F,R checkbox; 4=Continue */
    bool done = FALSE;

    while (!done) {
        /* Draw drive list */
        uint32_t y_start = 130;
        char buf[64];

        /* S Drive (always on) */
        draw_checkbox(60, y_start, "", TRUE, FALSE);
        strcpy(buf, "S: (System) - 35 GB [Fixed - Super Hidden]");
        gfx_draw_string(82, y_start + 1, buf, COLOR_LIGHT_GRAY, 1);

        /* D Drive */
        draw_checkbox(60, y_start + 30, "", g_config.drive_d, TRUE);
        gfx_fill_rect(82, y_start + 31, 400, 16, COLOR_BLUE);
        strcpy(buf, "D: (Data) - 4 GB");
        gfx_draw_string(82, y_start + 31, buf, sel == 0 ? COLOR_YELLOW : COLOR_WHITE, 1);

        /* E Drive */
        draw_checkbox(60, y_start + 60, "", g_config.drive_e, TRUE);
        gfx_fill_rect(82, y_start + 61, 400, 16, COLOR_BLUE);
        strcpy(buf, "E: (Extra) - 4 GB");
        gfx_draw_string(82, y_start + 61, buf, sel == 1 ? COLOR_YELLOW : COLOR_WHITE, 1);

        /* F Drive */
        draw_checkbox(60, y_start + 90, "", g_config.drive_f, TRUE);
        gfx_fill_rect(82, y_start + 91, 400, 16, COLOR_BLUE);
        strcpy(buf, "F: (Files) - 4 GB");
        gfx_draw_string(82, y_start + 91, buf, sel == 2 ? COLOR_YELLOW : COLOR_WHITE, 1);

        /* R Drive (Recovery) */
        draw_checkbox(60, y_start + 120, "", g_config.drive_r, TRUE);
        gfx_fill_rect(82, y_start + 121, 400, 16, COLOR_BLUE);
        strcpy(buf, "R: (Recovery) - 4 GB");
        gfx_draw_string(82, y_start + 121, buf, sel == 3 ? COLOR_YELLOW : COLOR_WHITE, 1);

        /* Continue button */
        draw_button(gfx_width() / 2 - 60, y_start + 180, 120, 36,
                     "Continue", sel == 4 ? COLOR_LIGHT_BLUE : COLOR_DARK_BLUE,
                     COLOR_WHITE, sel == 4);

        /* Wait for key */
        uint8_t sc = kb_read_scancode();
        if (sc & 0x80) continue;

        switch (sc) {
            case KEY_UP:
                if (sel > 0) sel--;
                break;
            case KEY_DOWN:
                if (sel < 4) sel++;
                break;
            case KEY_SPACE:
            case KEY_ENTER:
                if (sel == 0) g_config.drive_d = !g_config.drive_d;
                else if (sel == 1) g_config.drive_e = !g_config.drive_e;
                else if (sel == 2) g_config.drive_f = !g_config.drive_f;
                else if (sel == 3) g_config.drive_r = !g_config.drive_r;
                else if (sel == 4) done = TRUE;
                break;
        }
    }

    g_state = STATE_SETUP_COPY_EXIT;
}

/* ==================== SETUP: COPY/EXIT SCREEN ==================== */

static void screen_setup_copy_exit(void) {
    gfx_clear(COLOR_BLUE);

    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Ready to Install", COLOR_WHITE, 1);

    gfx_draw_string(40, 80, "Partitions configured. Choose an action:", COLOR_WHITE, 1);

    gfx_draw_string(60, 120, "Drive S: 35 GB (System) - READY", COLOR_WHITE, 1);
    if (g_config.drive_d)
        gfx_draw_string(60, 140, "Drive D: 4 GB (Data) - READY", COLOR_WHITE, 1);
    if (g_config.drive_e)
        gfx_draw_string(60, 160, "Drive E: 4 GB (Extra) - READY", COLOR_WHITE, 1);
    if (g_config.drive_f)
        gfx_draw_string(60, 180, "Drive F: 4 GB (Files) - READY", COLOR_WHITE, 1);
    if (g_config.drive_r)
        gfx_draw_string(60, 200, "Drive R: 4 GB (Recovery) - READY", COLOR_WHITE, 1);

    int sel = 0;
    bool done = FALSE;

    while (!done) {
        uint32_t bx = gfx_width() / 2 - 150;
        uint32_t by = 260;
        draw_button(bx, by, 130, 40, "Copy Files",
                     sel == 0 ? COLOR_LIGHT_BLUE : COLOR_DARK_BLUE,
                     COLOR_WHITE, sel == 0);
        draw_button(bx + 170, by, 130, 40, "Exit",
                     sel == 1 ? COLOR_RED : COLOR_DARK_GRAY,
                     COLOR_WHITE, sel == 1);

        uint8_t sc = kb_read_scancode();
        if (sc & 0x80) continue;

        switch (sc) {
            case KEY_LEFT:
            case KEY_RIGHT:
            case KEY_TAB:
                sel = 1 - sel;
                break;
            case KEY_ENTER:
            case KEY_SPACE:
                done = TRUE;
                break;
        }
    }

    if (sel == 1) {
        /* Exit: 5 second countdown then clean wipe and shutdown */
        gfx_clear(COLOR_BLUE);
        gfx_draw_string_centered(gfx_height() / 2 - 40,
                                  "Cleaning up and shutting down...", COLOR_WHITE, 1);
        for (int i = 5; i > 0; i--) {
            char buf[32];
            itoa(i, buf, 10);
            gfx_fill_rect(gfx_width()/2 - 20, gfx_height()/2, 40, 30, COLOR_BLUE);
            gfx_draw_string_centered(gfx_height() / 2, buf, COLOR_WHITE, 2);
            delay(1000);
        }
        gfx_draw_string_centered(gfx_height() / 2 + 40,
                                  "All data wiped. Shutting down.", COLOR_WHITE, 1);
        delay(500);
        g_state = STATE_SHUTDOWN;
    } else {
        g_state = STATE_SETUP_COPYING;
    }
}

/* ==================== SETUP: COPYING FILES ==================== */

static void screen_setup_copying(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Copying Files", COLOR_WHITE, 1);

    /* File list to "copy" */
    const char *files[] = {
        "bootloader.bin", "undo.bin", "security.sys", "bioshw.bin",
        "kernel/bin/osis_core.sys", "kernel/bin/interface.exe",
        "kernel/bin/command.exe", "kernel/bin/uc.exe",
        "kernel/bin/security.exe", "kernel/bin/settings.exe",
        "system64/hal.sys", "system64/drivers.sys",
        "system_x86/compat.sys",
        "kernel/logs/emergency.log",
        "os/shell.sys", "os/filesystem.sys", "os/memory.sys",
        "recovery/emergency/restore.sys",
        "recovery/S/backup.bin",
        "program_x64/system.dll", "program_x86/compat.dll",
        "programdata/config.dat",
        "Temp/",
    };
    int num_files = sizeof(files) / sizeof(files[0]);

    uint32_t bar_x = 60;
    uint32_t bar_y = gfx_height() / 2;
    uint32_t bar_w = gfx_width() - 120;
    uint32_t bar_h = 30;

    for (int i = 0; i < num_files; i++) {
        uint32_t progress = (i + 1) * 100 / num_files;

        /* File name - top left of progress bar */
        gfx_fill_rect(bar_x, bar_y - 25, bar_w, 20, COLOR_BLUE);
        gfx_draw_string(bar_x, bar_y - 22, files[i], COLOR_WHITE, 1);

        /* Estimated time - top right of progress bar */
        char time_buf[32];
        int remaining = (num_files - i) * 2;
        strcpy(time_buf, "~");
        char num[8];
        itoa(remaining, num, 10);
        strcpy(time_buf + 1, num);
        strcpy(time_buf + strlen(time_buf), "s remaining");
        uint32_t tw = strlen(time_buf) * 8;
        gfx_fill_rect(bar_x + bar_w - tw - 10, bar_y - 25, tw + 10, 20, COLOR_BLUE);
        gfx_draw_string(bar_x + bar_w - tw, bar_y - 22, time_buf, COLOR_LIGHT_GRAY, 1);

        /* Progress bar */
        gfx_draw_progress_bar(bar_x, bar_y, bar_w, bar_h, progress,
                               COLOR_LIGHT_BLUE, COLOR_DARK_BLUE);

        /* Percentage */
        char pct[8];
        itoa(progress, pct, 10);
        strcpy(pct + strlen(pct), "%");
        gfx_fill_rect(bar_x + bar_w / 2 - 20, bar_y + bar_h + 5, 50, 20, COLOR_BLUE);
        gfx_draw_string(bar_x + bar_w / 2 - 12, bar_y + bar_h + 5, pct, COLOR_WHITE, 1);

        delay(300);
    }

    gfx_draw_string_centered(bar_y + bar_h + 40, "Files copied successfully!",
                              COLOR_WHITE, 1);
    delay(500);
    g_state = STATE_SETUP_RESTART_PROMPT;
}

/* ==================== SETUP: RESTART PROMPT ==================== */

static void screen_setup_restart(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Copy Complete", COLOR_WHITE, 1);

    gfx_draw_string_centered(gfx_height() / 2 - 40,
                              "All files have been copied successfully.", COLOR_WHITE, 1);
    gfx_draw_string_centered(gfx_height() / 2 - 10,
                              "Please remove installation media if attached.", COLOR_WHITE, 1);

    draw_button(gfx_width() / 2 - 60, gfx_height() / 2 + 40, 120, 36,
                 "Restart", COLOR_LIGHT_BLUE, COLOR_WHITE, TRUE);

    /* Wait for Enter */
    while (1) {
        uint8_t sc = kb_read_scancode();
        if (!(sc & 0x80) && sc == KEY_ENTER) break;
    }

    /* Simulate restart - go to computer name setup */
    gfx_clear(COLOR_BLACK);
    gfx_draw_string_centered(gfx_height() / 2, "Restarting...", COLOR_WHITE, 1);
    delay(1500);
    g_state = STATE_SETUP_COMPUTER_NAME;
}

/* ==================== SETUP: COMPUTER NAME ==================== */

typedef struct {
    uint32_t x;
    uint32_t y;
    int pos;
} EchoCtx;

static void echo_char_to_field(char c, void *ctx) {
    EchoCtx *ec = (EchoCtx *)ctx;
    if (c == '\b') {
        if (ec->pos > 0) {
            ec->pos--;
            gfx_fill_rect(ec->x + 6 + ec->pos * 8, ec->y + 4, 10, 20, COLOR_WHITE);
        }
    } else if (c == '\n') {
        /* Done */
    } else {
        gfx_draw_char(ec->x + 6 + ec->pos * 8, ec->y + 6, c, COLOR_BLACK, 1);
        ec->pos++;
    }
}

static void screen_setup_computer_name(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Computer Name", COLOR_WHITE, 1);

    gfx_draw_string(60, 100, "Enter a name for this computer:", COLOR_WHITE, 1);

    uint32_t fx = 60, fy = 140;
    draw_input_field(fx, fy, 300, "", "My-OSIS-PC", TRUE, FALSE);

    draw_button(gfx_width() / 2 - 40, 200, 80, 32, "Next", COLOR_LIGHT_BLUE, COLOR_WHITE, TRUE);

    EchoCtx ec = {fx, fy, 0};
    kb_read_line(g_config.computer_name, 63, echo_char_to_field, &ec);

    if (strlen(g_config.computer_name) == 0) {
        strcpy(g_config.computer_name, "OSIS-PC");
    }

    g_state = STATE_SETUP_FONT_SELECT;
}

/* ==================== SETUP: FONT SELECTION ==================== */

static void screen_setup_font_select(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Font Selection", COLOR_WHITE, 1);

    gfx_draw_string(60, 80, "Select a display font (can be changed in Settings):", COLOR_WHITE, 1);

    const char *fonts[] = {"System Default (8x16)", "Monospace", "Terminal", "Sans"};
    int num_fonts = 4;
    int sel = 0;

    bool done = FALSE;
    while (!done) {
        for (int i = 0; i < num_fonts; i++) {
            uint32_t y = 120 + i * 30;
            gfx_fill_rect(60, y, 300, 24, COLOR_BLUE);

            if (i == sel) {
                gfx_fill_rounded_rect(58, y - 2, 304, 28, 3, COLOR_DARK_BLUE);
                gfx_draw_string(80, y + 4, fonts[i], COLOR_YELLOW, 1);
                gfx_draw_string(62, y + 4, ">", COLOR_WHITE, 1);
            } else {
                gfx_draw_string(80, y + 4, fonts[i], COLOR_WHITE, 1);
            }
        }

        draw_button(gfx_width() / 2 - 40, 280, 80, 32, "Next",
                     COLOR_LIGHT_BLUE, COLOR_WHITE, TRUE);

        uint8_t sc = kb_read_scancode();
        if (sc & 0x80) continue;
        if (sc == KEY_UP && sel > 0) sel--;
        if (sc == KEY_DOWN && sel < num_fonts - 1) sel++;
        if (sc == KEY_ENTER) done = TRUE;
    }

    g_config.selected_font = sel;
    g_state = STATE_SETUP_USER_CREATE;
}

/* ==================== SETUP: USER CREATION ==================== */

static void screen_setup_user_create(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Create User", COLOR_WHITE, 1);

    /* Username */
    gfx_draw_string(60, 80, "Username:", COLOR_WHITE, 1);
    uint32_t ux = 60, uy = 100;
    draw_input_field(ux, uy, 300, "", "User", TRUE, FALSE);

    EchoCtx ec1 = {ux, uy, 0};
    kb_read_line(g_config.username, 63, echo_char_to_field, &ec1);

    if (strlen(g_config.username) == 0) {
        strcpy(g_config.username, "User");
    }

    /* Password */
    gfx_draw_string(60, 150, "Password (leave empty for no password):", COLOR_WHITE, 1);
    uint32_t px = 60, py = 170;
    draw_input_field(px, py, 300, "", "Optional", TRUE, TRUE);

    /* For password, mask the echo */
    kb_read_line(g_config.password, 63, NULL, NULL);

    g_config.has_password = (strlen(g_config.password) > 0);

    gfx_draw_string(60, 220, "Press Enter to continue...", COLOR_LIGHT_GRAY, 1);
    draw_button(gfx_width() / 2 - 40, 260, 80, 32, "Next",
                 COLOR_LIGHT_BLUE, COLOR_WHITE, TRUE);

    /* Wait for Enter (password input already consumed it) */
    g_state = STATE_SETUP_INSTALLING;
}

/* ==================== SETUP: INSTALLING SYSTEM ==================== */

static void screen_setup_installing(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Installing", COLOR_WHITE, 1);

    const char *steps[] = {
        "Creating user profile...",
        "Creating Desktop folder...",
        "Creating Downloads folder...",
        "Creating Photos folder...",
        "Creating Videos folder...",
        "Creating Documents folder...",
        "Installing interface.exe...",
        "Installing command.exe...",
        "Installing uc.exe...",
        "Configuring computer name...",
        "Setting user credentials...",
        "Building folder structure...",
        "Installing system services...",
        "Configuring security policies...",
        "Finalizing installation...",
    };
    int num_steps = sizeof(steps) / sizeof(steps[0]);

    uint32_t bar_x = 80;
    uint32_t bar_y = gfx_height() / 2;
    uint32_t bar_w = gfx_width() - 160;

    for (int i = 0; i < num_steps; i++) {
        gfx_fill_rect(bar_x, bar_y - 25, bar_w, 20, COLOR_BLUE);
        gfx_draw_string(bar_x, bar_y - 22, steps[i], COLOR_WHITE, 1);
        gfx_draw_progress_bar(bar_x, bar_y, bar_w, 26,
                               (i + 1) * 100 / num_steps,
                               COLOR_LIGHT_BLUE, COLOR_DARK_BLUE);
        delay(400);
    }

    gfx_draw_string_centered(bar_y + 50, "Installation complete!", COLOR_WHITE, 1);
    delay(500);
    g_state = STATE_SETUP_RESTART2;
}

/* ==================== SETUP: SECOND RESTART ==================== */

static void screen_setup_restart2(void) {
    gfx_clear(COLOR_BLUE);
    gfx_fill_rect(0, 0, gfx_width(), 40, COLOR_DARK_BLUE);
    gfx_draw_string(20, 12, "OSIS Setup - Complete", COLOR_WHITE, 1);

    gfx_draw_string_centered(gfx_height() / 2 - 20,
                              "Setup is complete. Press Restart to continue.", COLOR_WHITE, 1);
    draw_button(gfx_width() / 2 - 60, gfx_height() / 2 + 30, 120, 36,
                 "Restart", COLOR_LIGHT_BLUE, COLOR_WHITE, TRUE);

    while (1) {
        uint8_t sc = kb_read_scancode();
        if (!(sc & 0x80) && sc == KEY_ENTER) break;
    }

    gfx_clear(COLOR_BLACK);
    gfx_draw_string_centered(gfx_height() / 2, "Restarting...", COLOR_WHITE, 1);
    delay(1500);
    g_state = STATE_SETUP_FINAL_INSTALL;
}

/* ==================== FINAL INSTALL (post-restart) ==================== */

static void screen_setup_final_install(void) {
    gfx_clear(COLOR_BLACK);

    /* Show installing remaining components */
    gfx_draw_string_centered(gfx_height() / 2 - 80,
                              "OSIS - Preparing your system", COLOR_WHITE, 2);

    const char *items[] = {
        "Installing selected font...",
        "Installing settings.exe...",
        "Installing security.exe...",
        "Setting desktop background...",
        "Configuring system clock...",
        "Starting services...",
    };
    int num = sizeof(items) / sizeof(items[0]);

    for (int i = 0; i < num; i++) {
        gfx_draw_string(60, gfx_height() / 2 - 20 + i * 20, items[i], COLOR_WHITE, 1);
        delay(500);
        gfx_draw_string(60 + strlen(items[i]) * 8 + 10,
                         gfx_height() / 2 - 20 + i * 20, "Done", COLOR_GREEN, 1);
    }

    delay(1000);
    g_config.setup_completed = TRUE;
    g_state = STATE_LOGIN_SELECT;
}

/* ==================== LOGIN: USER SELECT ==================== */

static void screen_login_select(void) {
    gfx_clear(COLOR_DARK_BLUE);

    /* OSIS logo at top */
    draw_osis_logo(gfx_width() / 2, 80, 30);

    gfx_draw_string_centered(130, "OSIS", COLOR_WHITE, 2);

    /* User icon (simple circle + body) */
    uint32_t ux = gfx_width() / 2;
    uint32_t uy = gfx_height() / 2 - 30;

    gfx_fill_circle(ux, uy - 20, 25, COLOR_LIGHT_GRAY);
    gfx_fill_rounded_rect(ux - 35, uy + 10, 70, 40, 10, COLOR_LIGHT_GRAY);

    /* Username */
    gfx_draw_string_centered(uy + 60, g_config.username, COLOR_WHITE, 2);

    gfx_draw_string_centered(uy + 90, "Press Enter to sign in", COLOR_LIGHT_GRAY, 1);

    while (1) {
        uint8_t sc = kb_read_scancode();
        if (!(sc & 0x80) && sc == KEY_ENTER) break;
    }

    if (g_config.has_password) {
        g_state = STATE_LOGIN_PASSWORD;
    } else {
        g_state = STATE_DESKTOP;
    }
}

/* ==================== LOGIN: PASSWORD ==================== */

static void screen_login_password(void) {
    gfx_clear(COLOR_DARK_BLUE);

    draw_osis_logo(gfx_width() / 2, 80, 30);
    gfx_draw_string_centered(130, g_config.username, COLOR_WHITE, 2);

    /* Password input */
    uint32_t px = gfx_width() / 2 - 150;
    uint32_t py = gfx_height() / 2 - 14;
    draw_input_field(px, py, 260, "", "Password", TRUE, TRUE);

    /* Green arrow button to submit */
    gfx_fill_rounded_rect(px + 265, py, 36, 28, 4, COLOR_GREEN);
    gfx_draw_string(px + 275, py + 6, ">", COLOR_WHITE, 1);

    char input[64];
    int tries = 0;
    while (tries < 3) {
        /* Clear field */
        gfx_fill_rect(px + 1, py + 1, 258, 26, COLOR_WHITE);

        /* Read password (characters masked) */
        int pos = 0;
        while (pos < 63) {
            char c = kb_get_char();
            if (c == '\n') {
                input[pos] = 0;
                break;
            }
            if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    gfx_fill_rect(px + 6 + pos * 8, py + 4, 10, 20, COLOR_WHITE);
                }
                continue;
            }
            input[pos] = c;
            gfx_draw_char(px + 6 + pos * 8, py + 6, '*', COLOR_BLACK, 1);
            pos++;
        }
        input[pos] = 0;

        if (strcmp(input, g_config.password) == 0) {
            g_state = STATE_DESKTOP;
            return;
        }

        tries++;
        gfx_fill_rect(px - 50, py + 35, 400, 20, COLOR_DARK_BLUE);
        gfx_draw_string(px, py + 35, "Incorrect password. Try again.", COLOR_RED, 1);
    }

    /* After 3 failed attempts */
    gfx_clear(COLOR_DARK_BLUE);
    gfx_draw_string_centered(gfx_height() / 2, "Too many attempts. Restarting...", COLOR_RED, 1);
    delay(3000);
    g_state = STATE_LOGIN_SELECT;
}

/* ==================== DESKTOP ==================== */

static void draw_desktop_background(void) {
    uint32_t w = gfx_width();
    uint32_t h = gfx_height() - 40; /* minus taskbar */

    /* Sunset sky - purple/orange gradient inspired by uploaded background */
    uint32_t sky_top = h * 15 / 100;
    uint32_t sky_mid = h * 40 / 100;
    uint32_t horizon = h * 55 / 100;
    uint32_t water_end = h * 70 / 100;

    /* Dark purple clouds at top */
    gfx_fill_gradient_v(0, 0, w, sky_top, 0x004B2060, 0x006B3070);

    /* Purple to orange sky */
    gfx_fill_gradient_v(0, sky_top, w, sky_mid - sky_top, 0x006B3070, 0x00D06030);

    /* Orange to golden horizon */
    gfx_fill_gradient_v(0, sky_mid, w, horizon - sky_mid, 0x00D06030, 0x00E8A020);

    /* Sun glow at horizon center */
    uint32_t sun_cx = w * 40 / 100;
    uint32_t sun_cy = horizon - 10;
    gfx_fill_circle(sun_cx, sun_cy, 40, 0x00F0C040);
    gfx_fill_circle(sun_cx, sun_cy - 5, 32, 0x00F8D860);

    /* Water/lake reflection - golden */
    gfx_fill_gradient_v(0, horizon, w, water_end - horizon, 0x00C08030, 0x00406030);

    /* Water shimmer lines */
    for (uint32_t y = horizon + 5; y < water_end; y += 8) {
        uint32_t lx = sun_cx - 60 + (y % 20);
        gfx_draw_line(lx, y, lx + 40, y, 0x00E0B050, 1);
    }

    /* Green ground with grass */
    gfx_fill_gradient_v(0, water_end, w, h - water_end, 0x00306820, 0x00204010);

    /* Flowers - small colored dots scattered on ground */
    uint32_t flower_colors[] = {0x00A040C0, 0x00E0E040, 0x00E06060, 0x00C060D0};
    for (uint32_t i = 0; i < 40; i++) {
        uint32_t fx = (i * 97 + 13) % w;
        uint32_t fy = water_end + 10 + ((i * 53 + 7) % (h - water_end - 15));
        gfx_fill_circle(fx, fy, 2, flower_colors[i % 4]);
    }

    /* Clouds - purple/dark streaks */
    gfx_fill_circle(w * 20 / 100, sky_top + 20, 35, 0x00553070);
    gfx_fill_circle(w * 25 / 100, sky_top + 15, 40, 0x00603878);
    gfx_fill_circle(w * 30 / 100, sky_top + 22, 30, 0x00553070);

    gfx_fill_circle(w * 60 / 100, sky_top + 30, 30, 0x00704080);
    gfx_fill_circle(w * 65 / 100, sky_top + 25, 35, 0x00603878);
    gfx_fill_circle(w * 70 / 100, sky_top + 32, 28, 0x00704080);

    /* Tree silhouette on right side */
    uint32_t tx = w * 70 / 100;
    uint32_t ty = water_end - 5;
    gfx_fill_rect(tx - 3, ty - 80, 6, 85, 0x00301820);
    gfx_fill_circle(tx, ty - 90, 25, 0x00402050);
    gfx_fill_circle(tx - 15, ty - 80, 18, 0x00402050);
    gfx_fill_circle(tx + 18, ty - 85, 20, 0x00402050);
    gfx_fill_circle(tx + 5, ty - 100, 15, 0x00402050);

    /* Small house silhouette on far right */
    uint32_t hx = w * 85 / 100;
    uint32_t hy = water_end;
    gfx_fill_rect(hx, hy - 25, 30, 25, 0x00403020);
    /* Roof */
    for (int i = 0; i < 15; i++) {
        gfx_draw_line(hx - i, hy - 25 - i, hx + 30 + i, hy - 25 - i, 0x00503828, 1);
    }
    /* Chimney smoke */
    gfx_fill_circle(hx + 25, hy - 45, 4, 0x00888888);
    gfx_fill_circle(hx + 27, hy - 52, 3, 0x00999999);
}

static void draw_taskbar(void) {
    uint32_t tb_y = gfx_height() - 40;
    gfx_fill_rect(0, tb_y, gfx_width(), 40, COLOR_TASKBAR);

    /* Start button */
    gfx_fill_rounded_rect(4, tb_y + 4, 70, 32, 4, COLOR_START_BTN);
    /* OSIS mini logo in start button (line through both top and bottom) */
    gfx_draw_line(24, tb_y + 8, 24, tb_y + 32, COLOR_WHITE, 1);
    gfx_draw_circle(24, tb_y + 20, 10, COLOR_WHITE, 1);
    gfx_draw_circle(24, tb_y + 20, 4, COLOR_WHITE, 1);
    gfx_draw_string(38, tb_y + 12, "OSIS", COLOR_WHITE, 1);

    /* Clock area on the right */
    gfx_draw_string(gfx_width() - 70, tb_y + 12, "12:00", COLOR_WHITE, 1);
}

/* Start menu */
static void draw_start_menu(void) {
    uint32_t mx = 4;
    uint32_t my = gfx_height() - 40 - 250;
    uint32_t mw = 220;
    uint32_t mh = 250;

    gfx_fill_rounded_rect(mx, my, mw, mh, 6, 0x002D2D30);

    /* Menu items */
    const char *items[] = {
        "Security.exe",
        "Settings.exe",
        "Command.exe",
        "UC (User Control)",
        "File Explorer",
    };
    int num = 5;

    for (int i = 0; i < num; i++) {
        uint32_t iy = my + 10 + i * 36;
        /* App icon (colored square) */
        uint32_t icon_colors[] = {COLOR_GREEN, COLOR_GRAY, COLOR_BLACK, COLOR_BLUE, COLOR_YELLOW};
        gfx_fill_rounded_rect(mx + 12, iy + 2, 24, 24, 3, icon_colors[i]);
        gfx_draw_string(mx + 44, iy + 6, items[i], COLOR_WHITE, 1);
    }

    /* Power options at bottom */
    gfx_draw_line(mx + 10, my + mh - 50, mx + mw - 10, my + mh - 50, COLOR_GRAY, 1);
    gfx_draw_string(mx + 12, my + mh - 40, "Sleep", COLOR_LIGHT_GRAY, 1);
    gfx_draw_string(mx + 70, my + mh - 40, "Restart", COLOR_LIGHT_GRAY, 1);
    gfx_draw_string(mx + 140, my + mh - 40, "Shutdown", COLOR_LIGHT_GRAY, 1);

    gfx_draw_string(mx + 12, my + mh - 18, g_config.username, COLOR_WHITE, 1);
}

/* Window drawing */
static void draw_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                         const char *title, bool active) {
    /* Title bar */
    uint32_t tb_color = active ? COLOR_WINDOW_TITLE : COLOR_DARK_GRAY;
    gfx_fill_rect(x, y, w, 30, tb_color);
    gfx_draw_string(x + 8, y + 7, title, COLOR_WHITE, 1);

    /* Close button (X) */
    gfx_fill_rect(x + w - 36, y, 36, 30, COLOR_RED);
    gfx_draw_string(x + w - 26, y + 7, "X", COLOR_WHITE, 1);

    /* Minimize & Maximize */
    gfx_fill_rect(x + w - 72, y, 36, 30, tb_color);
    gfx_draw_string(x + w - 58, y + 7, "=", COLOR_WHITE, 1);
    gfx_fill_rect(x + w - 108, y, 36, 30, tb_color);
    gfx_draw_string(x + w - 94, y + 7, "_", COLOR_WHITE, 1);

    /* Window body */
    gfx_fill_rect(x, y + 30, w, h - 30, COLOR_WINDOW_BG);
    gfx_draw_rect(x, y, w, h, COLOR_DARK_GRAY, 1);
}

/* Security.exe window content */
static void draw_security_window(uint32_t wx, uint32_t wy) {
    draw_window(wx, wy, 500, 350, "Security.exe - OSIS Security Center", TRUE);

    uint32_t cx = wx + 20;
    uint32_t cy = wy + 50;

    /* Real-time protection status */
    gfx_fill_circle(cx + 10, cy + 8, 8, COLOR_GREEN);
    gfx_draw_string(cx + 25, cy, "Real-Time Protection: ON", COLOR_BLACK, 1);

    cy += 35;
    gfx_draw_line(cx, cy, cx + 460, cy, COLOR_LIGHT_GRAY, 1);
    cy += 15;

    /* Scan buttons */
    draw_button(cx, cy, 140, 32, "Quick Scan", COLOR_BLUE, COLOR_WHITE, FALSE);
    draw_button(cx + 160, cy, 140, 32, "Full Scan", COLOR_DARK_BLUE, COLOR_WHITE, FALSE);

    cy += 50;
    gfx_draw_string(cx, cy, "Quick Scan: Checks running processes,", COLOR_DARK_GRAY, 1);
    gfx_draw_string(cx, cy + 18, "  background services, and memory.", COLOR_DARK_GRAY, 1);
    gfx_draw_string(cx, cy + 40, "Full Scan: Scans all drives, folders,", COLOR_DARK_GRAY, 1);
    gfx_draw_string(cx, cy + 58, "  and inspects .exe/.dll code deeply.", COLOR_DARK_GRAY, 1);

    cy += 90;
    gfx_draw_line(cx, cy, cx + 460, cy, COLOR_LIGHT_GRAY, 1);
    cy += 10;
    gfx_draw_string(cx, cy, "Last scan: Never", COLOR_GRAY, 1);
    gfx_draw_string(cx, cy + 18, "Threats found: 0", COLOR_GRAY, 1);
}

static void screen_desktop(void) {
    draw_desktop_background();
    draw_taskbar();

    /* Desktop icons */
    gfx_draw_string(20, 20, "Security.exe", COLOR_WHITE, 1);
    gfx_draw_string(20, 55, "Settings.exe", COLOR_WHITE, 1);
    gfx_draw_string(20, 90, "Command.exe", COLOR_WHITE, 1);

    /* Show a Security.exe window by default */
    draw_security_window(120, 80);

    /* Start menu toggle */
    bool menu_open = FALSE;
    bool running = TRUE;

    gfx_draw_string_centered(gfx_height() - 60,
                              "[S=Start] [Esc=GSOD Demo] [Q=Shutdown]", COLOR_LIGHT_GRAY, 1);

    while (running) {
        if (kb_has_key()) {
            uint8_t sc = inb(KB_DATA_PORT);
            if (sc & 0x80) continue;

            char c = scancode_to_ascii[sc];

            if (c == 's' || c == 'S') {
                menu_open = !menu_open;
                if (menu_open) {
                    draw_start_menu();
                } else {
                    /* Redraw desktop */
                    draw_desktop_background();
                    draw_taskbar();
                    draw_security_window(120, 80);
                    gfx_draw_string(20, 20, "Security.exe", COLOR_WHITE, 1);
                    gfx_draw_string(20, 55, "Settings.exe", COLOR_WHITE, 1);
                    gfx_draw_string(20, 90, "Command.exe", COLOR_WHITE, 1);
                    gfx_draw_string_centered(gfx_height() - 60,
                        "[S=Start] [Esc=GSOD Demo] [Q=Shutdown]", COLOR_LIGHT_GRAY, 1);
                }
            } else if (sc == KEY_ESCAPE) {
                g_state = STATE_GSOD;
                return;
            } else if (c == 'q' || c == 'Q') {
                g_state = STATE_SHUTDOWN;
                return;
            }
        }
    }
}

/* ==================== GREEN SCREEN OF DEATH ==================== */

static void screen_gsod(void) {
    gfx_clear(COLOR_GSOD_GREEN);

    /* Play error beep */
    speaker_on(800);
    delay(200);
    speaker_on(600);
    delay(200);
    speaker_off();

    uint32_t y = 60;
    uint32_t x = 60;

    /* Title */
    gfx_draw_string(x, y, "OSIS - Critical System Error", COLOR_WHITE, 2);
    y += 50;

    /* Error info with typewriter effect */
    const char *lines[] = {
        "A critical error has occurred. Error details:",
        "",
        "Error Code: 0x1900182",
        "Description: Service or critical folder was deleted",
        "Action: Restoring from S partition backup...",
        "",
        "Error Codes Reference:",
        "  0w100018  - Hardware disconnected during use",
        "  0x1900182 - Service/folder deleted (auto-restore)",
        "  0q19268   - Memory leak detected (buffer clear)",
        "  0xx00188826 - CRITICAL: Boot/disk wiped",
        "              -> Undo 5 Sec: repair MBR/GPT via undo.bin",
        "",
        "Emergency log saved to: /kernel/logs/emergency.log",
        "",
        "Press any key to attempt recovery...",
    };
    int num_lines = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < num_lines; i++) {
        /* Typewriter effect - draw char by char */
        const char *line = lines[i];
        for (int j = 0; line[j]; j++) {
            gfx_draw_char(x + j * 8, y, line[j], COLOR_WHITE, 1);
            /* Short delay for typewriter effect */
            for (volatile int d = 0; d < 50000; d++);
        }
        y += 20;
    }

    /* High-frequency beep for each character written (as spec says) */
    speaker_on(1200);
    delay(100);
    speaker_off();

    /* Wait for key */
    while (1) {
        uint8_t sc = kb_read_scancode();
        if (!(sc & 0x80)) break;
    }

    /* Recovery animation */
    gfx_clear(COLOR_GSOD_GREEN);
    gfx_draw_string_centered(gfx_height() / 2 - 40,
                              "Recovering system...", COLOR_WHITE, 2);

    for (int i = 0; i <= 100; i += 5) {
        gfx_draw_progress_bar(gfx_width() / 4, gfx_height() / 2,
                               gfx_width() / 2, 24, i,
                               COLOR_GREEN, COLOR_DARK_GREEN);
        delay(150);
    }

    gfx_draw_string_centered(gfx_height() / 2 + 40,
                              "Recovery complete. Returning to desktop.", COLOR_WHITE, 1);
    delay(1500);
    g_state = STATE_DESKTOP;
}

/* ==================== SHUTDOWN ==================== */

static void screen_shutdown(void) {
    gfx_clear(COLOR_BLACK);
    gfx_draw_string_centered(gfx_height() / 2 - 10, "Shutting down...", COLOR_WHITE, 2);
    delay(2000);
    gfx_clear(COLOR_BLACK);

    /* ACPI shutdown via I/O port (works with QEMU) */
    outw(0x604, 0x2000);

    /* Fallback: halt */
    while (1) {
        cli();
        hlt();
    }
}

/* ==================== KERNEL MAIN ==================== */

void kernel_main(BootInfo *boot_info) {
    g_boot_info = boot_info;

    /* Initialize graphics */
    gfx_init(boot_info);

    /* Initialize config */
    memset(&g_config, 0, sizeof(g_config));
    g_config.drive_s_size = 35;

    /* Main state machine loop */
    g_state = STATE_BOOT_LOGO;

    while (1) {
        switch (g_state) {
            case STATE_BOOT_LOGO:
                screen_boot_logo();
                break;
            case STATE_SETUP_PARTITION:
                screen_setup_partition();
                break;
            case STATE_SETUP_COPY_EXIT:
                screen_setup_copy_exit();
                break;
            case STATE_SETUP_COPYING:
                screen_setup_copying();
                break;
            case STATE_SETUP_RESTART_PROMPT:
                screen_setup_restart();
                break;
            case STATE_SETUP_COMPUTER_NAME:
                screen_setup_computer_name();
                break;
            case STATE_SETUP_FONT_SELECT:
                screen_setup_font_select();
                break;
            case STATE_SETUP_USER_CREATE:
                screen_setup_user_create();
                break;
            case STATE_SETUP_INSTALLING:
                screen_setup_installing();
                break;
            case STATE_SETUP_RESTART2:
                screen_setup_restart2();
                break;
            case STATE_SETUP_FINAL_INSTALL:
                screen_setup_final_install();
                break;
            case STATE_LOGIN_SELECT:
                screen_login_select();
                break;
            case STATE_LOGIN_PASSWORD:
                screen_login_password();
                break;
            case STATE_DESKTOP:
                screen_desktop();
                break;
            case STATE_GSOD:
                screen_gsod();
                break;
            case STATE_SHUTDOWN:
                screen_shutdown();
                break;
        }
    }
}
