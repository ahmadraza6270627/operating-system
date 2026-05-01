[org 0x7c00]

KERNEL_OFFSET equ 0x10000
KERNEL_LOAD_SEG equ 0x1000
KERNEL_SECTORS equ 250

    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov bp, 0x9000
    mov sp, bp
    sti

    mov [BOOT_DRIVE], dl

    mov bx, MSG_REAL_MODE
    call print
    call print_nl

    mov bx, MSG_LOAD_KERNEL
    call print
    call print_nl

    call load_kernel

    call switch_to_pm

    jmp $

%include "boot/print.asm"
%include "boot/load_sect.asm"
%include "boot/gdt.asm"
%include "boot/32bit_print.asm"
%include "boot/switch_pm.asm"

[bits 16]

load_kernel:
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx

    mov dh, KERNEL_SECTORS
    mov dl, [BOOT_DRIVE]
    call disk_load

    xor ax, ax
    mov es, ax

    ret

[bits 32]

BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm

    call KERNEL_OFFSET

    jmp $

BOOT_DRIVE db 0

MSG_REAL_MODE db "16-bit Real Mode", 0
MSG_LOAD_KERNEL db "Loading kernel", 0
MSG_PROT_MODE db "32-bit Protected Mode", 0

times 510-($-$$) db 0
dw 0xaa55