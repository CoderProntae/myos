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
    vesa_fill_screen(COLOR_BG);

    /* Pencere */
    int wx=150, wy=80, ww=500, wh=400;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Disk Denetleme", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

    int cy = wy + 50;

    /* Adim 1 */
    vesa_draw_string(wx+20, cy, "[1/4] Disk algilaniyor...", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    cy += 20;
    for (int p = 0; p <= 100; p += 4) {
        draw_progress(wx+20, cy, ww-40, p, COLOR_ACCENT);
        vesa_copy_buffer();
        delay(1);
    }
    vesa_draw_string(wx+20, cy-20, "[1/4] Disk algilandi      ", COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
    cy += 35;

    /* Adim 2 */
    vesa_draw_string(wx+20, cy, "[2/4] Sektorler okunuyor...", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    cy += 20;
    for (int p = 0; p <= 100; p += 3) {
        draw_progress(wx+20, cy, ww-40, p, 0x0099DD);
        vesa_copy_buffer();
        delay(1);
    }
    vesa_draw_string(wx+20, cy-20, "[2/4] Sektorler saglam     ", COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
    cy += 35;

    /* Adim 3 */
    vesa_draw_string(wx+20, cy, "[3/4] Dosya sistemi kontrol", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    cy += 20;
    for (int p = 0; p <= 100; p += 5) {
        draw_progress(wx+20, cy, ww-40, p, 0x44AA44);
        vesa_copy_buffer();
        delay(1);
    }
    vesa_draw_string(wx+20, cy-20, "[3/4] Dosya sistemi hazir  ", COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
    cy += 35;

    /* Adim 4 */
    vesa_draw_string(wx+20, cy, "[4/4] Alan hesaplaniyor...", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    cy += 20;
    for (int p = 0; p <= 100; p += 6) {
        draw_progress(wx+20, cy, ww-40, p, 0xCC8800);
        vesa_copy_buffer();
        delay(1);
    }
    vesa_draw_string(wx+20, cy-20, "[4/4] Yeterli alan mevcut ", COLOR_TEXT_GREEN, COLOR_WINDOW_BG);

    vesa_draw_string(wx+120, wy+wh-40, "Disk denetleme tamamlandi!", COLOR_TEXT_YELLOW, COLOR_WINDOW_BG);
    vesa_copy_buffer();
    delay(20);
}

static void install_screen(void) {
    vesa_fill_screen(COLOR_BG);

    int wx=150, wy=100, ww=500, wh=350;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "MyOS Kurulum", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

    const char* steps[] = {
        "Cekirdek kopyalaniyor...",
        "Sistem dosyalari ayarlaniyor...",
        "Suruculer yukleniyor...",
        "Masaustu hazirlaniyor...",
        "Yapilandirma tamamlaniyor...",
    };

    int cy = wy + 50;
    for (int s = 0; s < 5; s++) {
        vesa_draw_string(wx+20, cy, steps[s], COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
        cy += 20;
        for (int p = 0; p <= 100; p += 3) {
            draw_progress(wx+20, cy, ww-40, p, COLOR_ACCENT);
            vesa_copy_buffer();
            delay(1);
        }
        /* Tamamlandi */
        vesa_fill_rect(wx+20, cy-20, ww-40, 16, COLOR_WINDOW_BG);
        vesa_draw_string(wx+20, cy-20, steps[s], COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
        cy += 30;
    }

    vesa_draw_string(wx+100, wy+wh-40, "Kurulum tamamlandi! Baslatiliyor...", COLOR_TEXT_YELLOW, COLOR_WINDOW_BG);
    vesa_copy_buffer();
    delay(25);
}

int setup_run(void) {
    mouse_state_t ms;

    /* Hos geldin ekrani */
    vesa_fill_screen(COLOR_BG);

    int wx=200, wy=120, ww=400, wh=300;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);

    /* Logo */
    draw_windows_logo(wx+ww/2 - 8, wy+50, COLOR_ACCENT);
    vesa_draw_string(wx+ww/2 - 48, wy+80, "MyOS Setup", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2 - 80, wy+110, "Kurulum tipini secin:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);

    /* Buton: Grafik Kurulum */
    int bx=wx+80, by=wy+160, bw=240, bh=40;
    vesa_draw_rounded_rect(bx, by, bw, bh, COLOR_ACCENT, 4);
    vesa_draw_string(bx+48, by+12, "Grafik Kurulum", COLOR_TEXT_WHITE, COLOR_ACCENT);

    /* Buton: Diger */
    int bx2=wx+80, by2=wy+220, bw2=240, bh2=40;
    vesa_draw_rounded_rect(bx2, by2, bw2, bh2, COLOR_BUTTON, 4);
    vesa_draw_string(bx2+48, by2+12, "Diger (yakinda)", COLOR_TEXT_GREY, COLOR_BUTTON);

    vesa_copy_buffer();

    while (1) {
        mouse_poll(&ms);

        int hovering_install = (ms.x >= bx && ms.x < bx+bw && ms.y >= by && ms.y < by+bh);

        if (hovering_install)
            vesa_draw_rounded_rect(bx, by, bw, bh, COLOR_START_HOVER, 4);
        else
            vesa_draw_rounded_rect(bx, by, bw, bh, COLOR_ACCENT, 4);
        vesa_draw_string(bx+48, by+12, "Grafik Kurulum", COLOR_TEXT_WHITE,
                         hovering_install ? COLOR_START_HOVER : COLOR_ACCENT);

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        if (ms.click && hovering_install) {
            disk_check_screen();
            install_screen();
            return 1;
        }
    }
}
