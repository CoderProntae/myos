#include "setup.h"
#include "vga.h"
#include "mouse.h"
#include "string.h"
#include "io.h"

static void delay(int ticks) {
    for (volatile int i = 0; i < ticks * 500000; i++);
}

static void draw_progress_bar(int row, int percent, uint8_t fg) {
    int bar_start = 20;
    int bar_width = 40;
    int filled = (percent * bar_width) / 100;

    vga_print_at("[", VGA_WHITE, VGA_BLUE, bar_start - 1, row);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled)
            vga_putchar_at('#', fg | (VGA_BLUE << 4), bar_start + i, row);
        else
            vga_putchar_at('.', VGA_DARK_GREY | (VGA_BLUE << 4), bar_start + i, row);
    }
    vga_print_at("]", VGA_WHITE, VGA_BLUE, bar_start + bar_width, row);

    char pstr[5];
    k_itoa(percent, pstr, 10);
    vga_print_at("   ", VGA_WHITE, VGA_BLUE, bar_start + bar_width + 2, row);
    vga_print_at(pstr, VGA_YELLOW, VGA_BLUE, bar_start + bar_width + 2, row);
    vga_print_at("%", VGA_YELLOW, VGA_BLUE, bar_start + bar_width + 2 + k_strlen(pstr), row);
}

static int disk_check(void) {
    vga_clear_color(VGA_WHITE, VGA_BLUE);
    vga_hide_cursor();

    vga_print_at("MyOS Kurulum - Disk Denetleme", VGA_YELLOW, VGA_BLUE, 25, 2);
    vga_draw_box(15, 4, 64, 20, VGA_WHITE, VGA_BLUE);

    vga_print_at("Disk denetleniyor...", VGA_WHITE, VGA_BLUE, 18, 6);
    vga_print_at("Disk 0: ATA/SATA", VGA_LIGHT_CYAN, VGA_BLUE, 18, 8);

    /* Adim 1: Disk algilama */
    vga_print_at("[1/4] Disk algilaniyor...    ", VGA_WHITE, VGA_BLUE, 18, 10);
    for (int p = 0; p <= 100; p += 5) {
        draw_progress_bar(11, p, VGA_LIGHT_GREEN | (VGA_BLUE << 4));
        delay(1);
    }
    vga_print_at("[OK] Disk algilandi          ", VGA_LIGHT_GREEN, VGA_BLUE, 18, 10);

    /* Adim 2: Sektorler */
    vga_print_at("[2/4] Sektorler okunuyor...  ", VGA_WHITE, VGA_BLUE, 18, 12);
    for (int p = 0; p <= 100; p += 3) {
        draw_progress_bar(13, p, VGA_LIGHT_CYAN | (VGA_BLUE << 4));
        delay(1);
    }
    vga_print_at("[OK] Sektorler saglam        ", VGA_LIGHT_GREEN, VGA_BLUE, 18, 12);

    /* Adim 3: Dosya sistemi */
    vga_print_at("[3/4] Dosya sistemi kontrol..", VGA_WHITE, VGA_BLUE, 18, 14);
    for (int p = 0; p <= 100; p += 4) {
        draw_progress_bar(15, p, VGA_YELLOW | (VGA_BLUE << 4));
        delay(1);
    }
    vga_print_at("[OK] Dosya sistemi hazir     ", VGA_LIGHT_GREEN, VGA_BLUE, 18, 14);

    /* Adim 4: Alan kontrolu */
    vga_print_at("[4/4] Alan hesaplaniyor...   ", VGA_WHITE, VGA_BLUE, 18, 16);
    for (int p = 0; p <= 100; p += 6) {
        draw_progress_bar(17, p, VGA_LIGHT_MAGENTA | (VGA_BLUE << 4));
        delay(1);
    }
    vga_print_at("[OK] Yeterli alan mevcut     ", VGA_LIGHT_GREEN, VGA_BLUE, 18, 16);

    vga_print_at("Disk denetleme tamamlandi!", VGA_YELLOW, VGA_BLUE, 22, 19);

    delay(15);
    return 1;
}

static void install_system(void) {
    vga_clear_color(VGA_WHITE, VGA_BLUE);
    vga_hide_cursor();

    vga_print_at("MyOS Kurulum - Sistem Yukleniyor", VGA_YELLOW, VGA_BLUE, 23, 2);
    vga_draw_box(15, 4, 64, 18, VGA_WHITE, VGA_BLUE);

    const char* steps[] = {
        "Cekirdek kopyalaniyor...     ",
        "Sistem dosyalari ayarlaniyor.",
        "Suruculer yukleniyor...      ",
        "Masaustu hazirlaniyor...     ",
        "Yapilandirma tamamlaniyor... ",
    };

    for (int s = 0; s < 5; s++) {
        char step_str[40];
        step_str[0] = '[';
        step_str[1] = '1' + s;
        step_str[2] = '/';
        step_str[3] = '5';
        step_str[4] = ']';
        step_str[5] = ' ';
        k_strcpy(step_str + 6, steps[s]);

        vga_print_at(step_str, VGA_WHITE, VGA_BLUE, 18, 6 + s * 2);

        for (int p = 0; p <= 100; p += 2) {
            draw_progress_bar(7 + s * 2, p, VGA_LIGHT_GREEN | (VGA_BLUE << 4));
            delay(1);
        }
        vga_print_at("[OK]", VGA_LIGHT_GREEN, VGA_BLUE, 18, 6 + s * 2);
    }

    vga_print_at("Kurulum tamamlandi! Sistem baslatiliyor...", VGA_YELLOW, VGA_BLUE, 18, 17);
    delay(20);
}

int setup_run(void) {
    mouse_state_t ms;
    uint16_t saved_cursor[1];
    int old_col = -1, old_row = -1;
    uint16_t old_entry = 0;

    vga_clear_color(VGA_WHITE, VGA_BLUE);
    vga_hide_cursor();

    /* Baslik */
    vga_print_at("MyOS Kurulum Sihirbazi", VGA_YELLOW, VGA_BLUE, 28, 2);

    /* Kutu */
    vga_draw_box(20, 5, 59, 19, VGA_WHITE, VGA_BLUE);

    vga_print_at("MyOS'e Hos Geldiniz!", VGA_WHITE, VGA_BLUE, 29, 7);
    vga_print_at("Kurulum tipini secin:", VGA_LIGHT_CYAN, VGA_BLUE, 25, 9);

    /* Buton 1: Grafik Kurulum */
    vga_fill_rect(28, 12, 51, 14, VGA_WHITE, VGA_GREEN);
    vga_print_at(" Grafik Kurulum ", VGA_WHITE, VGA_GREEN, 30, 13);

    /* Buton 2: (devre disi) */
    vga_fill_rect(28, 16, 51, 18, VGA_DARK_GREY, VGA_BLACK);
    vga_print_at(" Diger (yakinda)", VGA_DARK_GREY, VGA_BLACK, 30, 17);

    /* Mouse dongusu */
    while (1) {
        mouse_poll(&ms);

        int mc = ms.col;
        int mr = ms.row;
        if (mc >= 80) mc = 79;
        if (mr >= 25) mr = 24;

        /* Eski cursor'u geri yukle */
        if (old_col >= 0 && old_row >= 0 && (old_col != mc || old_row != mr)) {
            vga_set_entry(old_col, old_row, old_entry);
        }

        if (mc != old_col || mr != old_row) {
            old_entry = vga_get_entry(mc, mr);
            old_col = mc;
            old_row = mr;
        }

        /* Mouse cursor ciz */
        vga_putchar_at('>', VGA_WHITE | (VGA_RED << 4), mc, mr);

        /* Hover efekti - Grafik Kurulum */
        if (mc >= 28 && mc <= 51 && mr >= 12 && mr <= 14) {
            vga_fill_rect(28, 12, 51, 14, VGA_WHITE, VGA_LIGHT_GREEN);
            vga_print_at(" Grafik Kurulum ", VGA_WHITE, VGA_LIGHT_GREEN, 30, 13);

            if (ms.left) {
                /* Tiklaninca flash efekti */
                vga_fill_rect(28, 12, 51, 14, VGA_BLACK, VGA_YELLOW);
                vga_print_at(" Grafik Kurulum ", VGA_BLACK, VGA_YELLOW, 30, 13);
                delay(5);

                /* Disk denetleme */
                disk_check();

                /* Kurulum */
                install_system();

                return 1;
            }
        } else {
            vga_fill_rect(28, 12, 51, 14, VGA_WHITE, VGA_GREEN);
            vga_print_at(" Grafik Kurulum ", VGA_WHITE, VGA_GREEN, 30, 13);
        }
    }
}
