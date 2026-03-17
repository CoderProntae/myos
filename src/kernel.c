#include "vesa.h"
#include "mouse.h"
#include "setup.h"
#include "gui.h"
#include "pci.h"
#include "e1000.h"
#include "net.h"
#include "dns.h"
#include "tcp.h"
#include "http.h"
#include "heap.h"
#include "vfs.h"
#include "elf.h"
#include "syscall.h"
#include "task.h"
#include "posix.h"
#include "gfx.h"

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    (void)magic;
    heap_init();

    if (mbi->flags & 0x01)
        ram_detect(mbi->mem_lower, mbi->mem_upper);

    vfs_init();
    elf_init();
    syscall_init();
    task_init();
    posix_init();
    vesa_init(mbi);
    gfx_init();
    mouse_init();
    pci_init();
    e1000_init();
    net_init();
    dns_init();
    tcp_init();
    http_init();

    setup_run();
    gui_run();
}
