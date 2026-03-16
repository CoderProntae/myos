#include "vesa.h"
#include "mouse.h"
#include "setup.h"
#include "gui.h"

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    (void)magic;
    vesa_init(mbi);
    mouse_init();

    setup_run();
    gui_run();
}
