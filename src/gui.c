#include "gui.h"
#include "vga.h"
#include "mouse.h"
#include "string.h"
#include "io.h"
#include "shell.h"

static int start_menu_open = 0;

static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static void draw_taskbar(void) {
    /* Gorev cubugu - en alt satir */
    vga_fill_row(24, VGA_WHITE, VGA_BLUE);

    /* Baslat butonu */
    if (start_menu_open)
        vga_print_at("[Baslat]", VGA_WHITE, VGA_LIGHT_BLUE, 0, 24);
    else
        vga_print_at("[Baslat]", VGA_WHITE, VGA_BLUE, 0, 24);

    /* Saat */
    outb(0x70, 0x04); uint8_t h = bcd_to_bin(inb(0x71));
    outb(0x70, 0x02); uint8_t m = bcd_to_bin(inb(0x71));

    char clock[6];
    clock[0] = '0' + h / 10; clock[1] = '0' + h % 10;
    clock[2] = ':';
    clock[3] = '0' + m / 10; clock[4] = '0' + m % 10;
    clock[5] = '\0';
    vga_print_at(clock, VGA_YELLOW, VGA_BLUE, 74, 24);

    /* MyOS yazisi ortada */
    vga_print_at("MyOS v0.2", VGA_LIGHT_CYAN, VGA_BLUE, 35, 24);
}

static void draw_start_menu(void) {
    vga_fill_rect(0, 16, 22, 23, VGA_WHITE, VGA_BLUE);
    vga_draw_box(0, 16, 22, 23, VGA_WHITE, VGA_BLUE);

    vga_print_at(" MyOS Menu       ", VGA_YELLOW, VGA_BLUE, 1, 17);
    vga_print_at(" Terminal        ", VGA_WHITE, VGA_BLUE, 1, 18);
    vga_print_at(" Sistem Bilgisi  ", VGA_WHITE, VGA_BLUE, 1, 19);
    vga_print_at(" Hakkinda        ", VGA_WHITE, VGA_BLUE, 1, 20);
    vga_print_at(" Yeniden Baslat  ", VGA_WHITE, VGA_BLUE, 1, 21);
    vga_print_at(" Kapat           ", VGA_WHITE, VGA_BLUE, 1, 22);
}

static void hide_start_menu(void) {
    /* Masaustu rengiyle temizle */
    for (int y = 16; y <= 23; y++)
        for (int x = 0; x <= 22; x++)
            vga_putchar_at(' ', VGA_WHITE | (VGA_CYAN << 4), x, y);
}

static void draw_desktop(void) {
    /* Masaustu - acik mavi/cyan arka plan */
    vga_clear_color(VGA_WHITE, VGA_CYAN);
    vga_hide_cursor();

    /* Masaustu ikonu */
    vga_print_at("[#] Bilgisayarim", VGA_WHITE, VGA_CYAN, 2, 2);
    vga_print_at("[>] Terminal", VGA_WHITE, VGA_CYAN, 2, 4);

    /* Hos geldin mesaji */
    vga_draw_box(22, 8, 57, 14, VGA_WHITE, VGA_BLUE);
    vga_print_at("MyOS Masaustune", VGA_YELLOW, VGA_BLUE, 30, 9);
    vga_print_at("Hos Geldiniz!", VGA_YELLOW, VGA_BLUE, 32, 10);
    vga_print_at("Mouse ile tiklayin.", VGA_WHITE, VGA_BLUE, 29, 12);
    vga_print_at("[Baslat] menuyu acin.", VGA_LIGHT_CYAN, VGA_BLUE, 28, 13);

    draw_taskbar();
}

static void show_sysinfo_window(void) {
    vga_draw_box(18, 5, 61, 18, VGA_WHITE, VGA_BLUE);
    vga_print_at(" Sistem Bilgisi ", VGA_YELLOW, VGA_BLUE, 30, 5);

    char vendor[13];
    uint32_t ebx, ecx, edx;
    __asm__ __volatile__("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *((uint32_t*)&vendor[0]) = ebx;
    *((uint32_t*)&vendor[4]) = edx;
    *((uint32_t*)&vendor[8]) = ecx;
    vendor[12] = '\0';

    vga_print_at("CPU     :", VGA_WHITE, VGA_BLUE, 20, 7);
    vga_print_at(vendor, VGA_LIGHT_GREEN, VGA_BLUE, 30, 7);
    vga_print_at("Mimari  : i686 32-bit", VGA_WHITE, VGA_BLUE, 20, 9);
    vga_print_at("VGA     : 80x25 text mode", VGA_WHITE, VGA_BLUE, 20, 11);
    vga_print_at("Versiyon: MyOS v0.2", VGA_WHITE, VGA_BLUE, 20, 13);
    vga_print_at("Klavye  : PS/2", VGA_WHITE, VGA_BLUE, 20, 15);

    vga_print_at("[Kapat - tikla]", VGA_YELLOW, VGA_BLUE, 32, 17);
}

static void show_about_window(void) {
    vga_draw_box(18, 5, 61, 16, VGA_WHITE, VGA_BLUE);
    vga_print_at(" Hakkinda ", VGA_YELLOW, VGA_BLUE, 34, 5);

    vga_print_at("  __  __        ___  ____", VGA_LIGHT_CYAN, VGA_BLUE, 24, 7);
    vga_print_at(" |  \\/  |_   _ / _ \\/ ___|", VGA_LIGHT_CYAN, VGA_BLUE, 23, 8);
    vga_print_at(" | |\\/| | | | | | | \\___ \\", VGA_LIGHT_BLUE, VGA_BLUE, 23, 9);
    vga_print_at(" | |  | | |_| | |_| |___) |", VGA_LIGHT_BLUE, VGA_BLUE, 23, 10);
    vga_print_at(" |_|  |_|\\__, |\\___/|____/", VGA_WHITE, VGA_BLUE, 23, 11);
    vga_print_at("         |___/", VGA_WHITE, VGA_BLUE, 23, 12);

    vga_print_at("MyOS v0.2 - Sifirdan OS", VGA_WHITE, VGA_BLUE, 26, 14);
    vga_print_at("[Kapat - tikla]", VGA_YELLOW, VGA_BLUE, 32, 15);
}

void gui_run(void) {
    mouse_state_t ms;
    int old_col = -1, old_row = -1;
    uint16_t old_entry = 0;
    int window_open = 0;   /* 0=yok, 1=sysinfo, 2=about */
    int click_cooldown = 0;

    draw_desktop();

    while (1) {
        mouse_poll(&ms);

        int mc = ms.col;
        int mr = ms.row;
        if (mc >= 80) mc = 79;
        if (mr >= 25) mr = 24;

        if (click_cooldown > 0) click_cooldown--;

        /* Eski cursor geri yukle */
        if (old_col >= 0 && old_row >= 0 && (old_col != mc || old_row != mr)) {
            vga_set_entry(old_col, old_row, old_entry);
        }

        if (mc != old_col || mr != old_row) {
            old_entry = vga_get_entry(mc, mr);
            old_col = mc;
            old_row = mr;
        }

        /* Mouse cursor */
        vga_putchar_at('>', VGA_WHITE | (VGA_RED << 4), mc, mr);

        /* Hover: Baslat butonu */
        if (mc >= 0 && mc <= 7 && mr == 24) {
            vga_print_at("[Baslat]", VGA_YELLOW, VGA_LIGHT_BLUE, 0, 24);
        } else {
            if (start_menu_open)
                vga_print_at("[Baslat]", VGA_WHITE, VGA_LIGHT_BLUE, 0, 24);
            else
                vga_print_at("[Baslat]", VGA_WHITE, VGA_BLUE, 0, 24);
        }

        /* Hover: Start menu items */
        if (start_menu_open) {
            for (int i = 18; i <= 22; i++) {
                if (mr == i && mc >= 1 && mc <= 21) {
                    vga_fill_rect(1, i, 21, i, VGA_WHITE, VGA_LIGHT_BLUE);
                    const char* items[] = {" Terminal        "," Sistem Bilgisi  "," Hakkinda        "," Yeniden Baslat  "," Kapat           "};
                    vga_print_at(items[i-18], VGA_WHITE, VGA_LIGHT_BLUE, 1, i);
                } else {
                    const char* items[] = {" Terminal        "," Sistem Bilgisi  "," Hakkinda        "," Yeniden Baslat  "," Kapat           "};
                    vga_print_at(items[i-18], VGA_WHITE, VGA_BLUE, 1, i);
                }
            }
        }

        /* Tiklama */
        if (ms.left && click_cooldown == 0) {
            click_cooldown = 10000;

            /* Pencere aciksa, tikla kapat */
            if (window_open) {
                window_open = 0;
                start_menu_open = 0;
                draw_desktop();
                old_col = -1; old_row = -1;
                continue;
            }

            /* Baslat butonu */
            if (mc >= 0 && mc <= 7 && mr == 24) {
                start_menu_open = !start_menu_open;
                if (start_menu_open)
                    draw_start_menu();
                else
                    hide_start_menu();
                old_col = -1; old_row = -1;
                continue;
            }

            /* Menu: Terminal */
            if (start_menu_open && mr == 18 && mc >= 1 && mc <= 21) {
                start_menu_open = 0;
                vga_clear();
                vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
                vga_print("MyOS Terminal - 'exit' ile masaustune don\n\n");
                shell_run();
                draw_desktop();
                old_col = -1; old_row = -1;
                continue;
            }

            /* Menu: Sistem Bilgisi */
            if (start_menu_open && mr == 19 && mc >= 1 && mc <= 21) {
                start_menu_open = 0;
                draw_desktop();
                show_sysinfo_window();
                window_open = 1;
                old_col = -1; old_row = -1;
                continue;
            }

            /* Menu: Hakkinda */
            if (start_menu_open && mr == 20 && mc >= 1 && mc <= 21) {
                start_menu_open = 0;
                draw_desktop();
                show_about_window();
                window_open = 2;
                old_col = -1; old_row = -1;
                continue;
            }

            /* Menu: Yeniden Baslat */
            if (start_menu_open && mr == 21 && mc >= 1 && mc <= 21) {
                outb(0x64, 0xFE);
                continue;
            }

            /* Menu: Kapat */
            if (start_menu_open && mr == 22 && mc >= 1 && mc <= 21) {
                vga_clear_color(VGA_WHITE, VGA_BLACK);
                vga_print_at("Sistem kapatiliyor...", VGA_WHITE, VGA_BLACK, 28, 12);
                outb(0x604, 0x2000);
                outb(0xB004, 0x2000);
                outb(0x4004, 0x3400);
                __asm__ __volatile__("cli; hlt");
                continue;
            }

            /* Masaustu ikon: Bilgisayarim */
            if (mc >= 2 && mc <= 17 && mr == 2) {
                draw_desktop();
                show_sysinfo_window();
                window_open = 1;
                old_col = -1; old_row = -1;
                continue;
            }

            /* Masaustu ikon: Terminal */
            if (mc >= 2 && mc <= 13 && mr == 4) {
                vga_clear();
                vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
                vga_print("MyOS Terminal - 'exit' ile masaustune don\n\n");
                shell_run();
                draw_desktop();
                old_col = -1; old_row = -1;
                continue;
            }

            /* Bos yere tiklayinca menuyu kapat */
            if (start_menu_open) {
                start_menu_open = 0;
                hide_start_menu();
                old_col = -1; old_row = -1;
            }
        }

        /* Saati guncelle */
        draw_taskbar();
    }
}
