#include "mouse.h"
#include "vesa.h"
#include "io.h"

static int mx, my;
static int sw = 800, sh = 600;
static uint8_t prev_left = 0;
static int ready = 0;

/* Paket okuma durumu */
static int cycle = 0;
static uint8_t pkt[3];

static void wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}

static void wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}

static void cmd(uint8_t c) {
    wait_write();
    outb(0x64, 0xD4);
    wait_write();
    outb(0x60, c);
}

static uint8_t read_ack(void) {
    wait_read();
    return inb(0x60);
}

/* Tampon temizle */
static void flush(void) {
    for (int i = 0; i < 100; i++) {
        if (!(inb(0x64) & 1)) break;
        inb(0x60);
    }
}

void mouse_init(void) {
    flush();

    /* Aux port etkinlestir */
    wait_write();
    outb(0x64, 0xA8);

    /* Controller yapilandirma oku */
    wait_write();
    outb(0x64, 0x20);
    wait_read();
    uint8_t status = inb(0x60);

    /* IRQ12 etkinlestir, mouse clock etkinlestir */
    status |= 0x02;       /* IRQ12 enable */
    status &= ~0x20;      /* Mouse clock enable */

    wait_write();
    outb(0x64, 0x60);
    wait_write();
    outb(0x60, status);

    /* Mouse sifirla */
    cmd(0xFF);
    read_ack(); /* ACK */
    read_ack(); /* BAT result (0xAA) */
    read_ack(); /* Device ID (0x00) */

    /* Varsayilan ayarlara don */
    cmd(0xF6);
    read_ack();

    /* Veri akisini baslat */
    cmd(0xF4);
    read_ack();

    flush();

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
    prev_left = 0;
    cycle = 0;
    ready = 1;
}

void mouse_poll(mouse_state_t* state) {
    state->x = mx;
    state->y = my;
    state->left = 0;
    state->right = 0;
    state->middle = 0;
    state->click = 0;

    if (!ready) return;

    /* Butun bekleyen bytelari isle */
    int max_reads = 30;
    while (max_reads-- > 0) {
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) break;     /* Veri yok */
        if (!(st & 0x20)) {
            /* Klavye verisi - atla */
            inb(0x60);
            continue;
        }

        uint8_t b = inb(0x60);

        if (cycle == 0) {
            /* Byte 0: flags */
            /* Bit 3 her zaman 1 olmali (senkronizasyon) */
            if (!(b & 0x08)) {
                /* Senkronizasyon kaybi - bu byte'i atla */
                continue;
            }
            /* Overflow bitleri set ise paketi atla */
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
        else if (cycle == 2) {
            pkt[2] = b;
            cycle = 0;

            /* Paketi isle */
            int dx = (int)pkt[1];
            int dy = (int)pkt[2];

            /* Isaret genisletme (sign extension) */
            if (pkt[0] & 0x10) dx = dx - 256;
            if (pkt[0] & 0x20) dy = dy - 256;

            /* Hareket uygula */
            mx += dx;
            my -= dy;

            /* Sinirla */
            if (mx < 0) mx = 0;
            if (mx >= sw) mx = sw - 1;
            if (my < 0) my = 0;
            if (my >= sh) my = sh - 1;

            /* Buton durumu */
            state->left   = (pkt[0] & 0x01) ? 1 : 0;
            state->right  = (pkt[0] & 0x02) ? 1 : 0;
            state->middle = (pkt[0] & 0x04) ? 1 : 0;

            /* Click event: basildigi an */
            if (state->left && !prev_left) {
                state->click = 1;
            }
            prev_left = state->left;
        }
    }

    state->x = mx;
    state->y = my;
}

/* Ok isareti cursor - siyah kenarlıklı beyaz ok */
void mouse_draw_cursor(int cx, int cy) {
    static const uint8_t cursor[15][13] = {
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

    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 13; c++) {
            if (cursor[r][c] == 2)
                vesa_putpixel_raw(cx + c, cy + r, 0xFFFFFF);
            else if (cursor[r][c] == 1)
                vesa_putpixel_raw(cx + c, cy + r, 0x000000);
        }
    }
}
