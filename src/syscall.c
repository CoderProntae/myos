#include "syscall.h"
#include "idt.h"
#include "heap.h"
#include "vfs.h"
#include "vesa.h"
#include "keyboard.h"
#include "string.h"
#include "io.h"
#include "net.h"
#include "dns.h"
#include "tcp.h"

/* ======== IDT ======== */

static idt_entry_t idt[IDT_SIZE];
static idt_ptr_t   idtr;

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

static void idt_load(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint32_t)&idt;
    __asm__ __volatile__("lidt (%0)" : : "r"(&idtr));
}

/* ======== Syscall Handler (C tarafı) ======== */

syscall_result_t syscall_handle(uint32_t num, uint32_t arg1,
                                 uint32_t arg2, uint32_t arg3,
                                 uint32_t arg4) {
    syscall_result_t res;
    res.retval = 0;
    res.extra = 0;

    switch (num) {
        case SYS_EXIT:
            res.retval = (int32_t)arg1;
            break;

        case SYS_PRINT: {
            /* arg1 = string pointer */
            const char* str = (const char*)arg1;
            if (str) {
                vesa_draw_string(0, 0, str, 0xFFFFFF, 0x000000);
            }
            res.retval = 0;
            break;
        }

        case SYS_GETCHAR:
            res.retval = keyboard_getchar();
            break;

        case SYS_MALLOC:
            res.retval = (int32_t)(uint32_t)kmalloc(arg1);
            break;

        case SYS_FREE:
            kfree((void*)arg1);
            res.retval = 0;
            break;

        case SYS_OPEN: {
            const char* path = (const char*)arg1;
            res.retval = vfs_find_path(path);
            break;
        }

        case SYS_READ: {
            /* arg1=node, arg2=buffer, arg3=max_size */
            res.retval = vfs_read_file((int)arg1, (void*)arg2, arg3);
            break;
        }

        case SYS_WRITE: {
            /* arg1=node, arg2=data, arg3=size */
            res.retval = vfs_write_file((int)arg1, (const void*)arg2, arg3);
            break;
        }

        case SYS_CLOSE:
            res.retval = 0;
            break;

        case SYS_TIME: {
            outb(0x70, 0x04);
            uint8_t h = inb(0x71);
            outb(0x70, 0x02);
            uint8_t m = inb(0x71);
            outb(0x70, 0x00);
            uint8_t s = inb(0x71);
            /* BCD to binary */
            h = ((h >> 4) * 10) + (h & 0x0F);
            m = ((m >> 4) * 10) + (m & 0x0F);
            s = ((s >> 4) * 10) + (s & 0x0F);
            res.retval = (int32_t)(h * 3600 + m * 60 + s);
            break;
        }

        case SYS_SLEEP: {
            for (volatile uint32_t i = 0; i < arg1 * 500000; i++);
            res.retval = 0;
            break;
        }

        case SYS_PUTPIXEL:
            /* arg1=x, arg2=y, arg3=color */
            vesa_putpixel((int)arg1, (int)arg2, arg3);
            res.retval = 0;
            break;

        case SYS_SCREEN_W:
            res.retval = vesa_get_width();
            break;

        case SYS_SCREEN_H:
            res.retval = vesa_get_height();
            break;

        case SYS_FILL_RECT:
            /* arg1=x|(y<<16), arg2=w|(h<<16), arg3=color */
            vesa_fill_rect(
                (int)(arg1 & 0xFFFF),
                (int)(arg1 >> 16),
                (int)(arg2 & 0xFFFF),
                (int)(arg2 >> 16),
                arg3
            );
            res.retval = 0;
            break;

        case SYS_DRAW_STR:
            /* arg1=x|(y<<16), arg2=string, arg3=fg_color, arg4=bg_color */
            vesa_draw_string(
                (int)(arg1 & 0xFFFF),
                (int)(arg1 >> 16),
                (const char*)arg2,
                arg3, arg4
            );
            res.retval = 0;
            break;

        case SYS_FLUSH:
            vesa_copy_buffer();
            res.retval = 0;
            break;

        case SYS_DNS: {
            uint8_t ip[4];
            int ret = dns_resolve((const char*)arg1, ip);
            if (ret == 0) {
                res.retval = (int32_t)(
                    ((uint32_t)ip[0]) |
                    ((uint32_t)ip[1] << 8) |
                    ((uint32_t)ip[2] << 16) |
                    ((uint32_t)ip[3] << 24)
                );
            } else {
                res.retval = -1;
            }
            break;
        }

        case SYS_TCP_CONNECT: {
            uint8_t ip[4];
            ip[0] = (uint8_t)(arg1);
            ip[1] = (uint8_t)(arg1 >> 8);
            ip[2] = (uint8_t)(arg1 >> 16);
            ip[3] = (uint8_t)(arg1 >> 24);
            res.retval = tcp_connect(ip, (uint16_t)arg2);
            break;
        }

        case SYS_TCP_SEND:
            res.retval = tcp_send((int)arg1, (const void*)arg2, (uint16_t)arg3);
            break;

        case SYS_TCP_RECV:
            res.retval = tcp_recv((int)arg1, (void*)arg2, (uint16_t)arg3);
            break;

        case SYS_TCP_CLOSE:
            res.retval = tcp_close((int)arg1);
            break;

        case SYS_MEMINFO: {
            /* arg1: 0=total, 1=used, 2=free */
            if (arg1 == 0) res.retval = (int32_t)heap_get_total();
            else if (arg1 == 1) res.retval = (int32_t)heap_get_used();
            else if (arg1 == 2) res.retval = (int32_t)heap_get_free();
            else if (arg1 == 3) res.retval = (int32_t)(ram_get_total_kb() * 1024);
            break;
        }

        case SYS_MKDIR:
            res.retval = vfs_create_dir((const char*)arg1, (int)arg2);
            break;

        case SYS_DELETE:
            res.retval = vfs_delete((int)arg1);
            break;

        case SYS_STRLEN:
            res.retval = k_strlen((const char*)arg1);
            break;

        case SYS_STRCMP:
            res.retval = k_strcmp((const char*)arg1, (const char*)arg2);
            break;

        default:
            res.retval = -1;
            break;
    }

    return res;
}

/* ======== INT 0x80 Handler (ASM'den cagrilir) ======== */

/* Bu fonksiyon assembly interrupt handler'dan cagrilir */
int32_t syscall_dispatcher(uint32_t num, uint32_t a1, uint32_t a2,
                            uint32_t a3, uint32_t a4) {
    syscall_result_t res = syscall_handle(num, a1, a2, a3, a4);
    return res.retval;
}

/* ======== Basit INT 0x80 handler ======== */

/* C'de interrupt handler */
void int80_handler_c(void);
void int80_handler_c(void) {
    uint32_t num, a1, a2, a3, a4;
    __asm__ __volatile__(
        "movl %%eax, %0\n"
        "movl %%ebx, %1\n"
        "movl %%ecx, %2\n"
        "movl %%edx, %3\n"
        "movl %%esi, %4\n"
        : "=m"(num), "=m"(a1), "=m"(a2), "=m"(a3), "=m"(a4)
    );

    int32_t ret = syscall_dispatcher(num, a1, a2, a3, a4);
    __asm__ __volatile__("movl %0, %%eax" : : "r"(ret));
}

/* ======== Init ======== */

void syscall_init(void) {
    /* IDT sifirla */
    k_memset(idt, 0, sizeof(idt));

    /* INT 0x80 = syscall handler */
    /* Basit versiyon: handler adresi olarak C fonksiyonu kullan */
    /* Not: Gercek OS'te assembly stub gerekir — simdilik basit tutuyoruz */

    /* IDT'yi yukle (interrupt olmadan) */
    idt_load();
}
