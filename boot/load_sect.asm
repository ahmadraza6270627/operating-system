[bits 16]

disk_load:
    pusha

    ; Caller gives:
    ; ES:BX = destination address
    ; DH    = number of sectors to read
    ; DL    = boot drive
    ;
    ; This loader reads one sector at a time and supports
    ; crossing a 64 KB ES:BX boundary.

    mov si, dx
    shr si, 8

    mov ch, 0
    mov dh, 0
    mov cl, 2

disk_load_loop:
    cmp si, 0
    je disk_load_done

    mov ah, 0x02
    mov al, 0x01

    int 0x13

    jc disk_error

    dec si

    add bx, 512
    jnc disk_no_segment_wrap

    mov ax, es
    add ax, 0x1000
    mov es, ax

disk_no_segment_wrap:
    inc cl

    cmp cl, 19
    jl disk_load_loop

    mov cl, 1
    inc dh

    cmp dh, 2
    jl disk_load_loop

    mov dh, 0
    inc ch

    jmp disk_load_loop

disk_load_done:
    popa
    ret

disk_error:
    mov bx, DISK_ERROR_MSG
    call print
    call print_nl

    jmp $

DISK_ERROR_MSG db "Disk read error", 0