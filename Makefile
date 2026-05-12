# OSIS - Operating System Interface Security
# Build System

CC = gcc
LD = ld
NASM = nasm
XORRISO = xorriso

# EFI compiler settings
EFI_INC = /usr/include/efi
EFI_INC_ARCH = /usr/include/efi/x86_64
EFI_LIB = /usr/lib
GNU_EFI_LIB = /usr/lib

EFI_CFLAGS = -I$(EFI_INC) -I$(EFI_INC_ARCH) \
    -ffreestanding -fno-stack-protector -fpic -fshort-wchar \
    -mno-red-zone -DEFI_FUNCTION_WRAPPER -Wall -Wextra

EFI_LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_x86_64_efi.lds \
    -shared -Bsymbolic -L$(EFI_LIB) -L$(GNU_EFI_LIB)

# Kernel compiler settings
KERNEL_CFLAGS = -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
    -mno-mmx -mno-sse -mno-sse2 -nostdlib -nostdinc \
    -Wall -Wextra -O2 -mcmodel=large

KERNEL_LDFLAGS = -nostdlib -T kernel/linker.ld

# Output directories
BUILD = build
ISO_DIR = $(BUILD)/iso
EFI_BOOT = $(ISO_DIR)/EFI/BOOT

.PHONY: all clean bootloader kernel iso run

all: iso

# Create build directories
$(BUILD):
	mkdir -p $(BUILD)
	mkdir -p $(EFI_BOOT)
	mkdir -p $(ISO_DIR)/kernel
	mkdir -p $(ISO_DIR)/fonts
	mkdir -p $(ISO_DIR)/recovery/emergency
	mkdir -p $(ISO_DIR)/recovery/S

# ==================== BOOTLOADER ====================

$(BUILD)/boot.o: bootloader/boot.c | $(BUILD)
	$(CC) $(EFI_CFLAGS) -c -o $@ $<

$(BUILD)/boot.so: $(BUILD)/boot.o
	$(LD) $(EFI_LDFLAGS) /usr/lib/crt0-efi-x86_64.o $< -o $@ -lefi -lgnuefi

$(EFI_BOOT)/BOOTX64.EFI: $(BUILD)/boot.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
		-j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 $< $@

bootloader: $(EFI_BOOT)/BOOTX64.EFI

# ==================== KERNEL ====================

$(BUILD)/entry.o: kernel/entry.asm | $(BUILD)
	$(NASM) -f elf64 -o $@ $<

$(BUILD)/kernel.o: kernel/kernel.c kernel/types.h kernel/string.h \
    kernel/gfx/framebuffer.h kernel/io/port.h kernel/io/keyboard.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(BUILD)/framebuffer.o: kernel/gfx/framebuffer.c kernel/gfx/framebuffer.h \
    kernel/gfx/font8x16.h kernel/types.h kernel/string.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(BUILD)/osis_kernel.bin: $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/framebuffer.o
	$(LD) $(KERNEL_LDFLAGS) -o $(BUILD)/osis_kernel.elf $^
	objcopy -O binary $(BUILD)/osis_kernel.elf $@

$(ISO_DIR)/kernel/osis_kernel.bin: $(BUILD)/osis_kernel.bin
	cp $< $@

kernel: $(ISO_DIR)/kernel/osis_kernel.bin

# ==================== ISO ====================

# Create a default font placeholder
$(ISO_DIR)/fonts/default.fnt: | $(BUILD)
	echo "OSIS Default Font v1.0" > $@

# Create recovery files
$(ISO_DIR)/recovery/emergency/restore.sys: | $(BUILD)
	echo "OSIS Emergency Restore v1.0" > $@

$(ISO_DIR)/recovery/S/backup.bin: | $(BUILD)
	echo "OSIS Backup Image v1.0" > $@

# Create the bootable ISO
$(BUILD)/osis.iso: bootloader kernel $(ISO_DIR)/fonts/default.fnt \
    $(ISO_DIR)/recovery/emergency/restore.sys $(ISO_DIR)/recovery/S/backup.bin
	$(XORRISO) -as mkisofs \
		-o $@ \
		-iso-level 3 \
		-V "OSIS" \
		-e EFI/BOOT/BOOTX64.EFI \
		-no-emul-boot \
		$(ISO_DIR)

iso: $(BUILD)/osis.iso
	@echo ""
	@echo "========================================="
	@echo "  OSIS ISO built: $(BUILD)/osis.iso"
	@echo "  Run with: make run"
	@echo "========================================="

# ==================== RUN IN QEMU ====================

OVMF_CODE = /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS = /usr/share/OVMF/OVMF_VARS.fd

run: $(BUILD)/osis.iso
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/ovmf_vars.fd \
		-cdrom $(BUILD)/osis.iso \
		-m 1024M \
		-vga std \
		-serial stdio \
		-no-reboot

# Copy OVMF vars for writable access
$(BUILD)/ovmf_vars.fd: | $(BUILD)
	cp $(OVMF_VARS) $@

run-vnc: $(BUILD)/osis.iso $(BUILD)/ovmf_vars.fd
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/ovmf_vars.fd \
		-cdrom $(BUILD)/osis.iso \
		-m 1024M \
		-vga std \
		-display vnc=:0 \
		-serial stdio \
		-no-reboot

clean:
	rm -rf $(BUILD)
