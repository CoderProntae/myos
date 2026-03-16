#include "vga.h"
#include "gui.h"
#include "shell.h"

void kernel_main(void) {
    vga_init();

    int mode = 0;

    while (1) {
        if (mode == 0) {
            int r = gui_run();
            if (r == GUI_EXIT_TERMINAL) mode = 1;
        } else {
            int r = shell_run();
            if (r == SHELL_EXIT_GUI) mode = 0;
        }
    }
}
