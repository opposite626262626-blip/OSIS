/*
 * OSIS - Operating System Interface Security
 * UEFI Bootloader (bootloader.bin / BOOTX64.EFI)
 *
 * Responsibilities:
 *   - Display OSIS boot logo (circle with inner circle and line)
 *   - Show "Setup file for you" loading text
 *   - Initialize GOP (Graphics Output Protocol) for framebuffer
 *   - Load and jump to the OSIS kernel
 */

#include <efi.h>
#include <efilib.h>

/* Framebuffer info passed to kernel */
typedef struct {
    UINT64 framebuffer_base;
    UINT32 width;
    UINT32 height;
    UINT32 pixels_per_scanline;
    UINT32 pixel_format; /* 0=RGBX, 1=BGRX */
    UINT64 kernel_base;
    UINT64 kernel_size;
    UINT64 memory_map_addr;
    UINT64 memory_map_size;
    UINT64 memory_map_desc_size;
    UINT64 font_base;
    UINT64 font_size;
} __attribute__((packed)) BootInfo;

/* Simple pixel drawing via GOP framebuffer */
static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

static void put_pixel(UINT32 x, UINT32 y, UINT32 color) {
    if (!gop || x >= gop->Mode->Info->HorizontalResolution ||
        y >= gop->Mode->Info->VerticalResolution)
        return;
    UINT32 *fb = (UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
    fb[y * gop->Mode->Info->PixelsPerScanLine + x] = color;
}

static void fill_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 color) {
    for (UINT32 j = y; j < y + h; j++)
        for (UINT32 i = x; i < x + w; i++)
            put_pixel(i, j, color);
}

/* Bresenham circle */
static void draw_circle(INT32 cx, INT32 cy, INT32 r, UINT32 color, int thickness) {
    for (int t = 0; t < thickness; t++) {
        INT32 rr = r - t;
        if (rr <= 0) break;
        INT32 x = 0, y = rr;
        INT32 d = 3 - 2 * rr;
        while (x <= y) {
            put_pixel(cx + x, cy + y, color);
            put_pixel(cx - x, cy + y, color);
            put_pixel(cx + x, cy - y, color);
            put_pixel(cx - x, cy - y, color);
            put_pixel(cx + y, cy + x, color);
            put_pixel(cx - y, cy + x, color);
            put_pixel(cx + y, cy - x, color);
            put_pixel(cx - y, cy - x, color);
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

static void draw_line_v(INT32 x, INT32 y1, INT32 y2, UINT32 color, int thickness) {
    for (INT32 y = y1; y <= y2; y++)
        for (int t = -thickness/2; t <= thickness/2; t++)
            put_pixel(x + t, y, color);
}

/* Spinning animation frames */
static CHAR16 *spinner[] = {L"|", L"/", L"-", L"\\"};

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    UINTN handle_count = 0;
    EFI_HANDLE *handle_buffer = NULL;

    InitializeLib(ImageHandle, SystemTable);

    /* Clear screen */
    ST->ConOut->ClearScreen(ST->ConOut);
    ST->ConOut->SetAttribute(ST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);

    /* Locate GOP */
    status = BS->LocateHandleBuffer(ByProtocol, &gop_guid, NULL,
                                     &handle_count, &handle_buffer);
    if (EFI_ERROR(status) || handle_count == 0) {
        Print(L"OSIS: No Graphics Output Protocol found. Text mode boot.\r\n");
        gop = NULL;
    } else {
        status = BS->HandleProtocol(handle_buffer[0], &gop_guid, (void **)&gop);
        if (EFI_ERROR(status)) gop = NULL;
    }

    if (handle_buffer) BS->FreePool(handle_buffer);

    /* Try to set a good graphics mode */
    if (gop) {
        UINTN best_mode = gop->Mode->Mode;
        UINT32 best_w = 0;
        for (UINTN i = 0; i < gop->Mode->MaxMode; i++) {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
            UINTN info_size;
            status = gop->QueryMode(gop, i, &info_size, &info);
            if (!EFI_ERROR(status)) {
                /* Prefer 1024x768 or closest mode */
                if (info->HorizontalResolution >= 1024 &&
                    info->HorizontalResolution <= 1920 &&
                    info->HorizontalResolution > best_w) {
                    best_mode = i;
                    best_w = info->HorizontalResolution;
                }
            }
        }
        gop->SetMode(gop, best_mode);
    }

    /* Display OSIS boot logo */
    if (gop) {
        UINT32 w = gop->Mode->Info->HorizontalResolution;
        UINT32 h = gop->Mode->Info->VerticalResolution;
        INT32 cx = w / 2;
        INT32 cy = h / 2 - 40;
        INT32 r_outer = 60;
        INT32 r_inner = 25;

        /* Clear to black */
        fill_rect(0, 0, w, h, 0x00000000);

        /* Outer circle (white) */
        draw_circle(cx, cy, r_outer, 0x00FFFFFF, 3);

        /* Inner circle (white) */
        draw_circle(cx, cy, r_inner, 0x00FFFFFF, 2);

        /* Vertical line from center going down past outer circle */
        draw_line_v(cx, cy, cy + r_outer + 15, 0x00FFFFFF, 2);

        /* "Setup file for you" text below logo - use console positioning */
        /* We'll use GOP framebuffer text rendering for this */
        /* For now use console */
    }

    /* Print boot message using UEFI console */
    Print(L"\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n");
    Print(L"                    OSIS - Operating System Interface Security\r\n\r\n");
    Print(L"                           Setup file for you\r\n\r\n");

    /* Spinning animation for loading */
    for (int i = 0; i < 20; i++) {
        Print(L"\r                           Loading %s ", spinner[i % 4]);
        BS->Stall(250000); /* 250ms */
    }

    Print(L"\r\n\r\n                    Loading kernel...\r\n");

    /* Load kernel from filesystem */
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    status = BS->HandleProtocol(ImageHandle, &li_guid, (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Cannot get loaded image protocol\r\n");
        goto halt;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    status = BS->HandleProtocol(loaded_image->DeviceHandle, &fs_guid, (void **)&fs);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Cannot access filesystem\r\n");
        goto halt;
    }

    EFI_FILE_PROTOCOL *root;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Cannot open volume\r\n");
        goto halt;
    }

    /* Open kernel file */
    EFI_FILE_PROTOCOL *kernel_file;
    status = root->Open(root, &kernel_file, L"\\kernel\\osis_kernel.bin",
                        EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Cannot find \\kernel\\osis_kernel.bin\r\n");
        goto halt;
    }

    /* Get kernel file size */
    EFI_FILE_INFO *file_info;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 256;
    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    status = BS->AllocatePool(EfiLoaderData, info_size, (void **)&file_info);
    if (EFI_ERROR(status)) goto halt;

    status = kernel_file->GetInfo(kernel_file, &fi_guid, &info_size, file_info);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Cannot get kernel info\r\n");
        goto halt;
    }

    UINTN kernel_size = file_info->FileSize;
    Print(L"    Kernel size: %lu bytes\r\n", kernel_size);

    /* Allocate memory for kernel at a fixed address */
    UINTN kernel_pages = (kernel_size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS kernel_addr = 0x100000; /* 1MB */
    status = BS->AllocatePages(AllocateAddress, EfiLoaderData,
                               kernel_pages, &kernel_addr);
    if (EFI_ERROR(status)) {
        /* Try any address */
        status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                   kernel_pages, &kernel_addr);
        if (EFI_ERROR(status)) {
            Print(L"ERROR: Cannot allocate memory for kernel\r\n");
            goto halt;
        }
    }

    /* Read kernel into memory */
    status = kernel_file->Read(kernel_file, &kernel_size, (void *)kernel_addr);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Cannot read kernel\r\n");
        goto halt;
    }
    kernel_file->Close(kernel_file);

    Print(L"    Kernel loaded at 0x%lx\r\n", kernel_addr);

    /* Load font file */
    EFI_FILE_PROTOCOL *font_file;
    UINT64 font_addr = 0;
    UINT64 font_sz = 0;
    status = root->Open(root, &font_file, L"\\fonts\\default.fnt",
                        EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(status)) {
        info_size = sizeof(EFI_FILE_INFO) + 256;
        status = font_file->GetInfo(font_file, &fi_guid, &info_size, file_info);
        if (!EFI_ERROR(status)) {
            font_sz = file_info->FileSize;
            UINTN font_pages = (font_sz + 4095) / 4096;
            EFI_PHYSICAL_ADDRESS fa = 0;
            status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                       font_pages, &fa);
            if (!EFI_ERROR(status)) {
                UINTN rsz = font_sz;
                font_file->Read(font_file, &rsz, (void *)fa);
                font_addr = fa;
                Print(L"    Font loaded at 0x%lx (%lu bytes)\r\n", fa, font_sz);
            }
        }
        font_file->Close(font_file);
    } else {
        Print(L"    No font file found, using built-in font\r\n");
    }

    BS->FreePool(file_info);
    root->Close(root);

    /* Get memory map for ExitBootServices */
    UINTN map_size = 0, map_key, desc_size;
    UINT32 desc_version;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;

    BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
    map_size += 2 * desc_size; /* Extra space */
    status = BS->AllocatePool(EfiLoaderData, map_size, (void **)&memory_map);
    if (EFI_ERROR(status)) goto halt;

    status = BS->GetMemoryMap(&map_size, memory_map, &map_key, &desc_size,
                              &desc_version);
    if (EFI_ERROR(status)) goto halt;

    /* Prepare BootInfo structure for kernel */
    BootInfo *boot_info;
    status = BS->AllocatePool(EfiLoaderData, sizeof(BootInfo),
                              (void **)&boot_info);
    if (EFI_ERROR(status)) goto halt;

    if (gop) {
        boot_info->framebuffer_base = gop->Mode->FrameBufferBase;
        boot_info->width = gop->Mode->Info->HorizontalResolution;
        boot_info->height = gop->Mode->Info->VerticalResolution;
        boot_info->pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
        boot_info->pixel_format =
            (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) ? 1 : 0;
    } else {
        boot_info->framebuffer_base = 0;
        boot_info->width = 80;
        boot_info->height = 25;
        boot_info->pixels_per_scanline = 80;
        boot_info->pixel_format = 0;
    }
    boot_info->kernel_base = kernel_addr;
    boot_info->kernel_size = kernel_size;
    boot_info->memory_map_addr = (UINT64)(UINTN)memory_map;
    boot_info->memory_map_size = map_size;
    boot_info->memory_map_desc_size = desc_size;
    boot_info->font_base = font_addr;
    boot_info->font_size = font_sz;

    Print(L"\r\n    Framebuffer: %ux%u at 0x%lx\r\n",
          boot_info->width, boot_info->height, boot_info->framebuffer_base);
    Print(L"    Exiting boot services and jumping to kernel...\r\n");

    /* Re-get memory map (may have changed from allocations) */
    map_size = 0;
    BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
    map_size += 2 * desc_size;
    EFI_MEMORY_DESCRIPTOR *final_map;
    BS->AllocatePool(EfiLoaderData, map_size, (void **)&final_map);
    BS->GetMemoryMap(&map_size, final_map, &map_key, &desc_size, &desc_version);
    boot_info->memory_map_addr = (UINT64)(UINTN)final_map;
    boot_info->memory_map_size = map_size;
    boot_info->memory_map_desc_size = desc_size;

    /* Exit boot services */
    status = BS->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        /* Retry with fresh map */
        map_size = 0;
        BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
        map_size += 2 * desc_size;
        BS->AllocatePool(EfiLoaderData, map_size, (void **)&final_map);
        BS->GetMemoryMap(&map_size, final_map, &map_key, &desc_size, &desc_version);
        boot_info->memory_map_addr = (UINT64)(UINTN)final_map;
        boot_info->memory_map_size = map_size;
        status = BS->ExitBootServices(ImageHandle, map_key);
    }

    /* Jump to kernel - kernel entry point is at the start of the loaded binary */
    /* Kernel expects: RDI = pointer to BootInfo */
    typedef void (*kernel_entry_t)(BootInfo *);
    kernel_entry_t kernel_entry = (kernel_entry_t)kernel_addr;
    kernel_entry(boot_info);

    /* Should never reach here */
halt:
    Print(L"\r\nOSIS Boot halted. Press any key to reboot.\r\n");
    EFI_INPUT_KEY key;
    ST->ConIn->Reset(ST->ConIn, FALSE);
    while (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) == EFI_NOT_READY);

    /* Reboot */
    RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

    /* Infinite loop as fallback */
    while (1) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
