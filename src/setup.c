#include "setup.h"
#include "vesa.h"
#include "mouse.h"
#include "string.h"
#include "icons.h"

static void delay(int n) {
    for (volatile int i = 0; i < n * 300000; i++);
}

static void draw_progress(int x, int y, int w, int percent, uint32_t fg) {
    vesa_fill_rect(x, y, w, 20, COLOR_PROGRESS_BG);
    int filled = (percent * w) / 100;
    if (filled > 0)
        vesa_fill_rect(x, y, filled, 20, fg);
    vesa_draw_rect_outline(x, y, w, 20, COLOR_WINDOW_BORDER);
    char pstr[5];
    k_itoa(percent, pstr, 10);
    char buf[8];
    k_strcpy(buf, pstr);
    int len = k_strlen(buf);
    buf[len] = '%'; buf[len+1] = '\0';
    vesa_draw_string(x + w/2 - k_strlen(buf)*4, y+2, buf, COLOR_TEXT_WHITE, COLOR_PROGRESS_BG);
}

static void disk_check_screen(void) {
    int wx=150, wy=80, ww=500, wh=400;
    int cy;
    const char* ok_msgs[] = {
        "[1/4] Disk algilandi           ",
        "[2/4] Sektorler saglam         ",
        "[3/4] Dosya sistemi hazir      ",
        "[4/4] Yeterli alan mevcut      "
    };
    const char* run_msgs[] = {
        "[1/4] Disk algilaniyor...      ",
        "[2/4] Sektorler okunuyor...    ",
        "[3/4] Dosya sistemi kontrol... ",
        "[4/4] Alan hesaplaniyor...     "
    };
    uint32_t colors[] = {COLOR_ACCENT, 0x0099DD, 0x44AA44, 0xCC8800};
    int speeds[] = {4, 3, 5, 6};

    for (int step = 0; step < 4; step++) {
        for (int p = 0; p <= 100; p += speeds[step]) {
            vesa_fill_screen(COLOR_BG);
            vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
            vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
            vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
            vesa_draw_string(wx+16, wy+8, "Disk Denetleme", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
            vesa_draw_string(wx+20, wy+50, "Disk 0: ATA/SATA", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);

            cy = wy + 80;
            for (int prev = 0; prev < step; prev++) {
                vesa_draw_string(wx+20, cy, ok_msgs[prev], COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
                cy += 40;
            }
            vesa_draw_string(wx+20, cy, run_msgs[step], COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
            draw_progress(wx+20, cy+20, ww-40, p, colors[step]);

            vesa_copy_buffer();
            delay(1);
        }
    }
    /* Hepsi tamamlandi */
    vesa_fill_screen(COLOR_BG);
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Disk Denetleme", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    cy = wy + 80;
    for (int i = 0; i < 4; i++) {
        vesa_draw_string(wx+20, cy, ok_msgs[i], COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
        cy += 40;
    }
    vesa_draw_string(wx+100, wy+wh-50, "Disk denetleme tamamlandi!", COLOR_TEXT_YELLOW, COLOR_WINDOW_BG);
    vesa_copy_buffer();
    delay(20);
}

static void install_screen(void) {
    int wx=150, wy=100, ww=500, wh=350;
    const char* steps[] = {
        "Cekirdek kopyalaniyor...",
        "Sistem dosyalari ayarlaniyor...",
        "Suruculer yukleniyor...",
        "Masaustu hazirlaniyor...",
        "Yapilandirma tamamlaniyor...",
    };

    for (int s = 0; s < 5; s++) {
        for (int p = 0; p <= 100; p += 3) {
            vesa_fill_screen(COLOR_BG);
            vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
            vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
            vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
            vesa_draw_string(wx+16, wy+8, "MyOS Kurulum", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

            int cy = wy + 50;
            for (int prev = 0; prev < s; prev++) {
                vesa_draw_string(wx+20, cy, steps[prev], COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
                cy += 30;
            }
            vesa_draw_string(wx+20, cy, steps[s], COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
            draw_progress(wx+20, cy+20, ww-40, p, COLOR_ACCENT);

            vesa_copy_buffer();
            delay(1);
        }
    }

    vesa_fill_screen(COLOR_BG);
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "MyOS Kurulum", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    int cy2 = wy+50;
    for (int i = 0; i < 5; i++) {
        vesa_draw_string(wx+20, cy2, "[OK]", COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
        cy2 += 30;
    }
    vesa_draw_string(wx+80, wy+wh-50, "Kurulum tamamlandi! Baslatiliyor...", COLOR_TEXT_YELLOW, COLOR_WINDOW_BG);
    vesa_copy_buffer();
    delay(25);
}

int setup_run(void) {
    mouse_state_t ms;
    int wx=200, wy=120, ww=400, wh=300;
    int bx=wx+80, by=wy+160, bw=240, bh=40;
    int bx2=wx+80, by2=wy+220;

    while (1) {
        mouse_poll(&ms);
        int hover1 = (ms.x>=bx && ms.x<bx+bw && ms.y>=by && ms.y<by+bh);

        /* --- Her frame yeniden ciz --- */
        vesa_fill_screen(COLOR_BG);

        /* Pencere */
        vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
        vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);

        /* Logo */
        draw_windows_logo(wx+ww/2-8, wy+50, COLOR_ACCENT);
        vesa_draw_string(wx+ww/2-48, wy+80, "MyOS Setup", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
        vesa_draw_string(wx+ww/2-80, wy+110, "Kurulum tipini secin:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);

        /* Buton 1 */
        uint32_t b1c = hover1 ? COLOR_START_HOVER : COLOR_ACCENT;
        vesa_draw_rounded_rect(bx, by, bw, bh, b1c, 4);
        vesa_draw_string(bx+48, by+12, "Grafik Kurulum", COLOR_TEXT_WHITE, b1c);

        /* Buton 2 (devre disi) */
        vesa_draw_rounded_rect(bx2, by2, bw, bh, COLOR_BUTTON, 4);
        vesa_draw_string(bx2+48, by2+12, "Diger (yakinda)", COLOR_TEXT_GREY, COLOR_BUTTON);

        /* Mouse cursor */
        mouse_draw_cursor(ms.x, ms.y);

        vesa_copy_buffer();

        /* Tiklama */
        if (ms.click && hover1) {
            mouse_clear_click();
            disk_check_screen();
            install_screen();
            return 1;
        }
    }
}
