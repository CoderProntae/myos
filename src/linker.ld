/* ============================================
 *         MyOS Linker Script
 * ============================================ */

ENTRY(_start)

SECTIONS
{
    /* Kernel 1MB'dan başlar (GRUB standardı) */
    . = 1M;
    
    /* Multiboot header ve kod */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }
    
    /* Read-only veri */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Initialized veri */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }
    
    /* Uninitialized veri (BSS) */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Kernel sonu */
    _kernel_end = .;
    
    /* Debug bilgileri (opsiyonel) */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
