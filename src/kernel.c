#include "vesa.h"
#include "mouse.h"
#include "setup.h"
#include "gui.h"
#include "pci.h"
#include "e1000.h"
#include "net.h"
#include "dns.h"

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    (void)magic;
    vesa_init(mbi);
    mouse_init();
    pci_init();
    e1000_init();
    net_init();
    dns_init();

    setup_run();
    gui_run();
}
