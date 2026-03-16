#include "mouse.h"
#include "vesa.h"
#include "io.h"

static int mx, my;
static int sw = 800, sh = 600;
static uint8_t prev_left = 0;
static uint8_t prev_right = 0;
static int ready = 0;
static int cycle = 0;
static uint8_t pkt[3];

static void wait_w(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}

static void wait_r(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}

static void mcmd(uint8_t c) {
    wait_w(); outb(0x64, 0xD4);
    wait_w(); outb(0x60, c);
}

static uint8_t mread(void) {
    wait_r();
    return inb(0x60);
}

static void flush(void) {
    for (int i = 0; i < 100; i++) {
        if (!(inb(0x64) & 1)) break;
        inb(0x60);
    }
}

void mouse_init(void) {
    flush();

    /* Aux port etkinlestir */
    wait_w(); outb(0x64, 0xA8);

    /* Controller config */
    wait_w(); outb(0x64, 0x20);
    wait_r();
    uint8_t status = inb(0x60);
    status |= 0x02;
    status &= ~0x20;
    wait_w(); outb(0x64, 0x60);
    wait_w(); outb(0x60, status);

    /* Reset */
    mcmd(0xFF);
    mread(); mread(); mread();

    /* Defaults */
    mcmd(0xF6); mread();

    /* Enable */
    mcmd(0xF4); mread();

    flush();

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
    prev_left = 0;
    prev_right = 0;
    cycle = 0;
    ready = 1;
}

void mouse_poll(mouse_state_t* state) {
    /* Varsayilan degerler */
    state->x = mx;
    state->y = my;
    state->left = 0;
    state->right = 0;
    state->middle = 0;
    state->click = 0;
    state->right_click = 0;

    if (!ready) return;

    uint8_t got_left = 0;
    uint8_t got_right = 0;
    uint8_t got_middle = 0;
    int processed = 0;

    /* Tum bekleyen paketleri oku */
    int safety = 60;
    while (safety-- > 0) {
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) break;

        /* Klavye verisi - atla */
        if (!(st & 0x20)) {
            inb(0x60);
            continue;
        }

        uint8_t b = inb(0x60);

        if (cycle == 0) {
            /* Byte 0: flags - senkronizasyon kontrolu */
            if (!(b & 0x08)) {
                /* Bit 3 set degil = senkronizasyon kaybi */
                /* Bu byte'i atla, senkronizasyonu yeniden bul */
                continue;
            }
            /* Overflow varsa paketi atla */
            if (b & 0xC0) {
                continue;
            }
            pkt[0] = b;
            cycle = 1;
        }
        else if (cycle == 1) {
            pkt[1] = b;
            cycle = 2;
        }
        else {
            pkt[2] = b;
            cycle = 0;
            processed = 1;

            /* Delta hesapla */
            int dx = (int)(int8_t)pkt[1];
            int dy = (int)(int8_t)pkt[2];

            /* Sign bit duzeltme (PS/2 protokolu) */
            if (pkt[0] & 0x10) {
                if (dx > 0) dx = dx - 256;
            } else {
                if (dx < 0) dx = dx + 256;
            }
            if (pkt[0] & 0x20) {
                if (dy > 0) dy = dy - 256;
            } else {
                if (dy < 0) dy = dy + 256;
            }

            /* Sapma korumasi: cok buyuk degerleri sinirla */
            if (dx > 80) dx = 80;
            if (dx < -80) dx = -80;
            if (dy > 80) dy = 80;
            if (dy < -80) dy = -80;

            /* Pozisyon guncelle */
            mx += dx;
            my -= dy;

            /* Ekran sinirlari */
            if (mx < 0) mx = 0;
            if (mx >= sw) mx = sw - 1;
            if (my < 0) my = 0;
            if (my >= sh) my = sh - 1;

            /* Buton durumu */
            got_left   = (pkt[0] & 0x01) ? 1 : 0;
            got_right  = (pkt[0] & 0x02) ? 1 : 0;
            got_middle = (pkt[0] & 0x04) ? 1 : 0;
        }
    }

    /* Eger hic paket islenmemisse, onceki buton durumunu kullan */
    if (!processed) {
        got_left = prev_left;
        got_right = prev_right;
    }

    /* Butonlari kaydet */
    state->left = got_left;
    state->right = got_right;
    state->middle = got_middle;

    /* Click algilama: basildigi AN (edge detection) */
    if (got_left && !prev_left) {
        state->click = 1;
    }
    if (got_right && !prev_right) {
        state->right_click = 1;
    }

    prev_left = got_left;
    prev_right = got_right;

    /* Pozisyon guncelle */
    state->x = mx;
    state->y = my;
}

void mouse_draw_cursor(int cx, int cy) {
    static const uint8_t cur[15][13] = {
        {1,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,1,1,1,1,0,0,0},
        {1,2,2,1,2,2,1,0,0,0,0,0,0},
        {1,2,1,0,1,2,2,1,0,0,0,0,0},
        {1,1,0,0,1,2,2,1,0,0,0,0,0},
        {1,0,0,0,0,1,2,2,1,0,0,0,0},
        {0,0,0,0,0,1,1,1,0,0,0,0,0},
    };
    for (int r = 0; r < 15; r++)
        for (int c = 0; c < 13; c++) {
            if (cur[r][c] == 2)
                vesa_putpixel_raw(cx + c, cy + r, 0xFFFFFF);
            else if (cur[r][c] == 1)
                vesa_putpixel_raw(cx + c, cy + r, 0x000000);
        }
}
