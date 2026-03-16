#include "vga.h"
#include "setup.h"

void kernel_main(void) {
    vga_init();
    vga_hide_cursor();
    setup_run();

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
