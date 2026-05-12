; OSIS Kernel Entry Point (x86-64)
; This is the first code that runs when the bootloader jumps to the kernel.
; Sets up a basic environment and calls the C kernel_main.

[BITS 64]
[GLOBAL _start]
[EXTERN kernel_main]

section .text

_start:
    ; RDI already contains pointer to BootInfo from bootloader
    ; Set up a simple stack
    mov rsp, stack_top

    ; Clear direction flag
    cld

    ; Call C kernel_main(BootInfo *boot_info)
    call kernel_main

    ; If kernel returns, halt
.halt:
    cli
    hlt
    jmp .halt

section .bss
    align 16
    resb 65536      ; 64KB stack
stack_top:
