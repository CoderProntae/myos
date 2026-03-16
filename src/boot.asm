MBALIGN    equ 1 << 0
MEMINFO    equ 1 << 1
VIDMODE    equ 1 << 2
FLAGS      equ MBALIGN | MEMINFO | VIDMODE
MAGIC      equ 0x1BADB002
CHECKSUM   equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0       ; aout kludge (unused)
    dd 0                    ; mode_type: 0 = linear graphics
    dd 800                  ; width
    dd 600                  ; height
    dd 32                   ; depth (32-bit color)

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx                ; multiboot info pointer
    push eax                ; magic number
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
