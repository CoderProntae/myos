; ============================================
;         MyOS Boot - Multiboot
; ============================================

; Multiboot sabitleri
MBALIGN     equ 1 << 0                 ; Sayfa hizalama
MEMINFO     equ 1 << 1                 ; Bellek bilgisi
VIDMODE     equ 1 << 2                 ; Video modu bilgisi
FLAGS       equ MBALIGN | MEMINFO | VIDMODE
MAGIC       equ 0x1BADB002             ; Multiboot magic
CHECKSUM    equ -(MAGIC + FLAGS)

; Multiboot header
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    
    ; aout kludge (boş)
    dd 0    ; header_addr
    dd 0    ; load_addr
    dd 0    ; load_end_addr
    dd 0    ; bss_end_addr
    dd 0    ; entry_addr
    
    ; Video mode
    dd 0    ; mode_type (0 = linear graphics)
    dd 800  ; width
    dd 600  ; height
    dd 32   ; depth (bpp)

; Stack
section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KB stack
stack_top:

; Boot kodu
section .text
global _start
extern kernel_main

_start:
    ; Stack pointer'ı ayarla
    mov esp, stack_top
    
    ; EFLAGS temizle
    push 0
    popf
    
    ; kernel_main(multiboot_info_t* mboot, uint32_t magic)
    ; C calling convention: parametreler stack'e sağdan sola
    push eax            ; magic number (EAX = 0x2BADB002)
    push ebx            ; multiboot info pointer (EBX)
    
    ; kernel_main çağır
    call kernel_main
    
    ; kernel_main dönerse (dönmemeli)
    cli
.hang:
    hlt
    jmp .hang
