#include "setup.h"
#include "vga.h"
#include "mouse.h"
#include "gui.h"
#include "io.h"
#include <stdint.h>

typedef struct {
    int x, y, w, h;
    const char* label;
    uint8_t fg, bg;
    uint8_t hover_fg, hover_bg;
    int enabled;
} sbutton_t;

static int str_len(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void fill_rect(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            vga_putentry_at(' ', fg, bg, xx, yy);
}

static void draw_frame(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int xx = x; xx < x + w; xx++) {
        vga_putentry_at(0xC4, fg, bg, xx, y);
        vga_putentry_at(0xC4, fg, bg, xx, y + h - 1);
    }
    for (int yy = y; yy < y + h; yy++) {
        vga_putentry_at(0xB3, fg, bg, x, yy);
        vga_putentry_at(0xB3, fg, bg, x + w - 1, yy);
    }
    vga_putentry_at(0xDA, fg, bg, x, y);
    vga_putentry_at(0xBF, fg, bg, x + w - 1, y);
    vga_putentry_at(0xC0, fg, bg, x, y + h - 1);
    vga_putentry_at(0xD9, fg, bg, x + w - 1, y + h - 1);
}

static void draw_sbutton(const sbutton_t* b, int hover) {
    uint8_t fg, bg;
    if (!b->enabled) {
        fg = VGA_DARK_GREY;
        bg = VGA_BLACK;
    } else if (hover) {
        fg = b->hover_fg;
        bg = b->hover_bg;
    } else {
        fg = b->fg;
        bg = b->bg;
    }
    fill_rect(b->x, b->y, b->w, b->h, fg, bg);
    draw_frame(b->x, b->y, b->w, b->h, fg, bg);
    int len = str_len(b->label);
    int tx = b->x + (b->w - len) / 2;
    int ty = b->y + b->h / 2;
    vga_write_at(b->label, tx, ty, fg, bg);
}

static int inside(const sbutton_t* b, int x, int y) {
    return (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h);
}

static int cursor_drawn = 0;
static int cdx = 0, cdy = 0;
static uint16_t csaved = 0;

static void cur_hide(void) {
    if (!cursor_drawn) return;
    vga_setentry_at(csaved, cdx, cdy);
    cursor_drawn = 0;
}

static void cur_show(int x, int y) {
    cur_hide();
    csaved = vga_getentry_at(x, y);
    uint8_t col = (uint8_t)(csaved >> 8);
    uint8_t ofg = col & 0x0F;
    uint8_t obg = (col >> 4) & 0x0F;
    char c = (char)(csaved & 0xFF);
    if (!c || c == ' ') c = ' ';
    vga_putentry_at(c, obg, ofg, x, y);
    cdx = x; cdy = y; cursor_drawn = 1;
}

static void status_bar(const char* msg) {
    fill_rect(0, 24, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(msg, 2, 24, VGA_WHITE, VGA_DARK_GREY);
}

/* ===================== STEP 0: Boot Menu ===================== */

static int step0_boot_menu(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();

    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Setup ", 2, 0, VGA_WHITE, VGA_DARK_GREY);

    fill_rect(15, 5, 50, 13, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(15, 5, 50, 13, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(16, 6, 48, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" MyOS Kurulum Sihirbazi ", 27, 6, VGA_WHITE, VGA_BLUE);

    vga_write_at("Hosgeldiniz!", 30, 9, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Kurulum yontemini secin:", 25, 11, VGA_BLACK, VGA_LIGHT_GREY);

    sbutton_t btn_gui = {20, 14, 40, 3,
        "Grafik Arayuzu ile Kurulum",
        VGA_WHITE, VGA_GREEN, VGA_WHITE, VGA_LIGHT_GREEN, 1};

    draw_sbutton(&btn_gui, 0);
    status_bar("Kurulum yontemini secin.");

    mouse_state_t ms;
    mouse_get_state(&ms);
    cur_show(ms.x, ms.y);
    int last_left = 0;

    while (1) {
        if (mouse_poll(&ms)) {
            cur_hide();
            int h = inside(&btn_gui, ms.x, ms.y);
            draw_sbutton(&btn_gui, h);
            if (h) status_bar("Grafik arayuzu ile kuruluma basla.");
            else   status_bar("Kurulum yontemini secin.");
            if (ms.left && !last_left && h && btn_gui.enabled) {
                cur_hide();
                return 1;
            }
            cur_show(ms.x, ms.y);
            last_left = ms.left;
        }
    }
}

/* ===================== STEP 1: Welcome ===================== */

static int step1_welcome(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();

    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Setup - Adim 1/4 ", 2, 0, VGA_WHITE, VGA_DARK_GREY);

    fill_rect(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(11, 4, 58, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" Hosgeldiniz ", 30, 4, VGA_WHITE, VGA_BLUE);

    vga_write_at("MyOS kurulumuna hosgeldiniz.", 14, 7, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Bu sihirbaz sizi adim adim", 14, 9, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("kurulum surecinden gecirecek.", 14, 10, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Bu bir TEST kurulumudur.", 14, 12, VGA_DARK_GREY, VGA_LIGHT_GREY);
    vga_write_at("Gercek disk islemi yapilmaz.", 14, 13, VGA_DARK_GREY, VGA_LIGHT_GREY);

    sbutton_t btn_next = {50, 16, 16, 3, "Ileri >>",
        VGA_WHITE, VGA_GREEN, VGA_WHITE, VGA_LIGHT_GREEN, 1};
    draw_sbutton(&btn_next, 0);
    status_bar("Devam etmek icin 'Ileri' tusuna basin.");

    mouse_state_t ms;
    mouse_get_state(&ms);
    cur_show(ms.x, ms.y);
    int last_left = 0;

    while (1) {
        if (mouse_poll(&ms)) {
            cur_hide();
            int h = inside(&btn_next, ms.x, ms.y);
            draw_sbutton(&btn_next, h);
            cur_show(ms.x, ms.y);
            if (ms.left && !last_left && h) { cur_hide(); return 1; }
            last_left = ms.left;
        }
    }
}

/* ===================== STEP 2: Disk Selection ===================== */

static int step2_disk(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();

    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Setup - Adim 2/4 ", 2, 0, VGA_WHITE, VGA_DARK_GREY);

    fill_rect(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(11, 4, 58, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" Disk Secimi ", 30, 4, VGA_WHITE, VGA_BLUE);

    vga_write_at("Kurulum diski (simule):", 14, 7, VGA_BLACK, VGA_LIGHT_GREY);

    fill_rect(14, 9, 52, 3, VGA_WHITE, VGA_CYAN);
    draw_frame(14, 9, 52, 3, VGA_WHITE, VGA_CYAN);
    vga_write_at("[*] /dev/sda - 64 GB (Sanal Disk)", 16, 10, VGA_WHITE, VGA_CYAN);

    vga_write_at("(Test modu: gercek disk islemi yok)", 14, 13, VGA_DARK_GREY, VGA_LIGHT_GREY);

    sbutton_t btn_next = {50, 16, 16, 3, "Ileri >>",
        VGA_WHITE, VGA_GREEN, VGA_WHITE, VGA_LIGHT_GREEN, 1};
    sbutton_t btn_back = {14, 16, 16, 3, "<< Geri",
        VGA_WHITE, VGA_BROWN, VGA_BLACK, VGA_YELLOW, 1};

    draw_sbutton(&btn_next, 0);
    draw_sbutton(&btn_back, 0);
    status_bar("Disk secildi. Devam edin.");

    mouse_state_t ms;
    mouse_get_state(&ms);
    cur_show(ms.x, ms.y);
    int last_left = 0;

    while (1) {
        if (mouse_poll(&ms)) {
            cur_hide();
            int hn = inside(&btn_next, ms.x, ms.y);
            int hb = inside(&btn_back, ms.x, ms.y);
            draw_sbutton(&btn_next, hn);
            draw_sbutton(&btn_back, hb);
            cur_show(ms.x, ms.y);
            if (ms.left && !last_left) {
                if (hn) { cur_hide(); return 1; }
                if (hb) { cur_hide(); return -1; }
            }
            last_left = ms.left;
        }
    }
}

/* ===================== STEP 3: Install Progress ===================== */

static void fake_delay(int ticks) {
    for (volatile int i = 0; i < ticks * 500000; i++);
}

static void draw_progress(int pct) {
    int bar_w = 50;
    int filled = (pct * bar_w) / 100;
    char buf[4];

    for (int i = 0; i < bar_w; i++) {
        if (i < filled)
            vga_putentry_at(0xDB, VGA_GREEN, VGA_BLACK, 15 + i, 12);
        else
            vga_putentry_at(0xB0, VGA_DARK_GREY, VGA_BLACK, 15 + i, 12);
    }

    buf[0] = '0' + (pct / 100) % 10;
    buf[1] = '0' + (pct / 10) % 10;
    buf[2] = '0' + pct % 10;
    buf[3] = '\0';
    if (pct < 100) buf[0] = ' ';
    if (pct < 10)  buf[1] = ' ';

    vga_write_at("   ", 66, 12, VGA_WHITE, VGA_BLUE);
    vga_write_at(buf, 66, 12, VGA_WHITE, VGA_BLUE);
    vga_write_at("%", 69, 12, VGA_WHITE, VGA_BLUE);
}

static int step3_install(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();

    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Setup - Adim 3/4 ", 2, 0, VGA_WHITE, VGA_DARK_GREY);

    fill_rect(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(11, 4, 58, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" Kurulum ", 32, 4, VGA_WHITE, VGA_BLUE);

    vga_write_at("MyOS kuruluyor...", 14, 7, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("(Simule ediliyor, disk islemi yok)", 14, 8, VGA_DARK_GREY, VGA_LIGHT_GREY);

    const char* steps[] = {
        "Dosya sistemi olusturuluyor...",
        "Temel sistem kopyalaniyor...",
        "Cekirdek kuruluyor...",
        "Suruculer yukleniyor...",
        "Bootloader ayarlaniyor...",
        "Yapilandirma tamamlaniyor...",
    };
    int nsteps = 6;

    for (int i = 0; i < nsteps; i++) {
        fill_rect(14, 10, 52, 1, VGA_BLACK, VGA_LIGHT_GREY);
        vga_write_at(steps[i], 14, 10, VGA_BLACK, VGA_LIGHT_GREY);
        status_bar(steps[i]);

        int start_pct = (i * 100) / nsteps;
        int end_pct = ((i + 1) * 100) / nsteps;
        if (i == nsteps - 1) end_pct = 100;

        for (int p = start_pct; p <= end_pct; p++) {
            draw_progress(p);
            fake_delay(2);
        }
    }

    fill_rect(14, 10, 52, 1, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Kurulum tamamlandi!", 14, 10, VGA_GREEN, VGA_LIGHT_GREY);
    draw_progress(100);
    status_bar("Kurulum tamamlandi!");

    sbutton_t btn_next = {50, 16, 16, 3, "Ileri >>",
        VGA_WHITE, VGA_GREEN, VGA_WHITE, VGA_LIGHT_GREEN, 1};
    draw_sbutton(&btn_next, 0);

    mouse_state_t ms;
    mouse_get_state(&ms);
    cur_show(ms.x, ms.y);
    int last_left = 0;

    while (1) {
        if (mouse_poll(&ms)) {
            cur_hide();
            int h = inside(&btn_next, ms.x, ms.y);
            draw_sbutton(&btn_next, h);
            cur_show(ms.x, ms.y);
            if (ms.left && !last_left && h) { cur_hide(); return 1; }
            last_left = ms.left;
        }
    }
}

/* ===================== STEP 4: Finish ===================== */

static int step4_finish(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();

    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Setup - Adim 4/4 ", 2, 0, VGA_WHITE, VGA_DARK_GREY);

    fill_rect(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(10, 3, 60, 16, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(11, 4, 58, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" Tamamlandi! ", 30, 4, VGA_WHITE, VGA_BLUE);

    vga_write_at("MyOS basariyla kuruldu!", 14, 7, VGA_GREEN, VGA_LIGHT_GREY);
    vga_write_at("Simdi masaustune gecebilirsiniz.", 14, 9, VGA_BLACK, VGA_LIGHT_GREY);

    sbutton_t btn_desktop = {25, 13, 30, 3, "Masaustune Gec",
        VGA_WHITE, VGA_GREEN, VGA_WHITE, VGA_LIGHT_GREEN, 1};
    sbutton_t btn_reboot = {25, 16, 30, 3, "Yeniden Baslat",
        VGA_WHITE, VGA_RED, VGA_WHITE, VGA_LIGHT_RED, 1};

    draw_sbutton(&btn_desktop, 0);
    draw_sbutton(&btn_reboot, 0);
    status_bar("Bir secenek secin.");

    mouse_state_t ms;
    mouse_get_state(&ms);
    cur_show(ms.x, ms.y);
    int last_left = 0;

    while (1) {
        if (mouse_poll(&ms)) {
            cur_hide();
            int hd = inside(&btn_desktop, ms.x, ms.y);
            int hr = inside(&btn_reboot, ms.x, ms.y);
            draw_sbutton(&btn_desktop, hd);
            draw_sbutton(&btn_reboot, hr);
            if (hd) status_bar("Masaustune gec.");
            else if (hr) status_bar("Sistemi yeniden baslat.");
            else status_bar("Bir secenek secin.");
            cur_show(ms.x, ms.y);
            if (ms.left && !last_left) {
                if (hd) { cur_hide(); return 1; }
                if (hr) { cur_hide(); return 2; }
            }
            last_left = ms.left;
        }
    }
}

/* ===================== MAIN SETUP ===================== */

void setup_run(void) {
    vga_hide_cursor();
    mouse_init();

    step0_boot_menu();

    int step = 1;
    while (1) {
        int result = 0;
        switch (step) {
            case 1: result = step1_welcome(); break;
            case 2: result = step2_disk();    break;
            case 3: result = step3_install(); break;
            case 4: result = step4_finish();  break;
            default: break;
        }

        if (step == 2 && result == -1) {
            step = 1;
            continue;
        }

        if (step == 4) {
            if (result == 1) {
                gui_run();
                return;
            } else if (result == 2) {
                outb(0x64, 0xFE);
                for (;;) __asm__ __volatile__("cli; hlt");
            }
        }

        step++;
        if (step > 4) step = 4;
    }
}
