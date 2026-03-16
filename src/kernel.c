#include "vga.h"
#include "mouse.h"
#include "setup.h"
#include "gui.h"

void kernel_main(void) {
    vga_init();
    mouse_init();

    /* Kurulum sihirbazi */
    setup_run();

    /* Masaustu */
    gui_run();
}
