# OSIS - Operating System Interface Security

A real bootable operating system built from scratch with UEFI boot support, custom kernel, graphical setup flow, desktop environment, and integrated security system.

## Features

### Boot & Setup Flow
- **UEFI Bootloader**: Custom EFI application with OSIS logo display and kernel loading
- **Setup Wizard**: Blue-screen setup interface with:
  - Automatic screen resolution detection
  - Drive partitioning (S/D/E/F/R drives)
  - File copy with progress bar
  - Computer name, font selection, user creation
  - Password protection (optional)

### Desktop Environment
- **Windows-like interface**: Taskbar with Start button, clock, overlapping windows
- **Background**: Sky and ground gradient with clouds
- **Start Menu**: Application launcher with power options (Sleep, Restart, Shutdown)
- **System Applications**:
  - `Security.exe` - Real-time protection, Quick Scan, Full Scan
  - `Settings.exe` - Font, user management, system configuration
  - `Command.exe` - System control (protected files cannot be deleted)
  - `UC (User Control)` - Run applications with elevated privileges

### Security System
- **Green Screen of Death (GSOD)**: Error display with typewriter effect and audio feedback
- **Error Codes**:
  - `0w100018` - Hardware disconnected during use
  - `0x1900182` - Service/folder deleted (auto-restore from S)
  - `0q19268` - Memory leak (buffer clear + service restart)
  - `0xx00188826` - Critical boot/disk wipe (Undo 5 Sec via `undo.bin`)
- **Emergency logging**: All errors saved to `/kernel/logs/emergency.log`

### Partition Structure
| Drive | Size | Purpose |
|-------|------|---------|
| S | 35 GB (min) | System (Super Hidden) - bootloader, undo.bin, security.sys, bioshw.bin |
| D | 4 GB+ | Data storage |
| E | 4 GB+ | Extra storage |
| F | 4 GB+ | Files |
| R | 4 GB+ | Recovery (emergency restore, S backup) |

### S Drive Contents
```
S:/
├── bootloader.bin      # UEFI boot code
├── undo.bin           # Disk snapshot for recovery (10 KB)
├── security.sys       # Access control & pre-desktop scanning
├── bioshw.bin         # BIOS communication (0/1 precision timing)
├── program_x64/       # 64-bit program files
├── program_x86/       # 32-bit program files
├── programdata/       # App data (combined appdata)
├── Temp/              # Downloads, updates, temp emergency logs
├── kernel/
│   ├── bin/           # System binaries
│   ├── logs/          # emergency.log, bios_reports
│   └── ...
├── system64/          # 64-bit system files
├── system_x86/        # x86 compatibility
├── os/                # Core OS files
└── recovery/
    ├── emergency/     # Emergency .sys files
    └── S/             # S partition backup code
```

## Hardware Requirements
- **CPU**: Intel Core 2 Duo or higher (x86-64)
- **RAM**: 890 MB minimum, up to 72 GB
- **Storage**: 35 GB minimum, up to 3 TB
- **Graphics**: VGA/GOP/HDMI support
- **Network**: Intel Ethernet or standard NIC

## Building

### Prerequisites
```bash
sudo apt-get install gcc nasm xorriso mtools qemu-system-x86 ovmf gnu-efi
```

### Build
```bash
make          # Build the ISO
make run      # Build and run in QEMU
make clean    # Clean build files
```

### Output
The build produces `build/osis.iso` - a standard bootable UEFI ISO image.

## Architecture
- **Bootloader**: Written in C using GNU-EFI, compiles to `BOOTX64.EFI`
- **Kernel**: Written in C with x86-64 Assembly entry point
- **Graphics**: Direct framebuffer rendering via UEFI GOP
- **Input**: PS/2 keyboard driver with scancode translation
- **No external dependencies**: Runs bare-metal, no libc or external libraries

## Supported File Formats
- `.exe` - Executable applications
- `.dll` - Dynamic libraries
- `.sys` - System drivers
- `.bin` - Binary data files

## License
OSIS is a custom operating system project.
