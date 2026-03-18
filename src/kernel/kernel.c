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

void kernel_main(multiboot_info_t* mboot, unsigned int magic) {

    volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) vga[i] = 0x0F00 | ' ';
    
    const char* msg = "MyOS v0.3 - Kernel yuklendi!";
    for (int i = 0; msg[i]; i++) vga[i] = 0x0A00 | msg[i];
    
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
    terminal_register();
    calculator_register();
    browser_register();
    settings_register();
    notepad_register();
    fm_register();
    about_register();

    setup_run();
    gui_run();

    while (1) {
        asm volatile ("hlt");
    }
}
