#include "mouse.h"
#include "vesa.h"
#include "io.h"

static int mx, my;
static int sw = 800, sh = 600;
static int ready = 0;
static int cycle = 0;
static uint8_t pkt[3];

/* Buton gecmisi */
static uint8_t prev_left = 0;
static uint8_t prev_right = 0;
static uint8_t cur_left = 0;
static uint8_t cur_right = 0;

/* Click latch — set edilir, poll SONUNDA dondurulur */
static uint8_t pending_click = 0;
static uint8_t pending_rclick = 0;

/* PS/2 yardimcilar */
static void ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}
static void ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}
static void ps2_mouse_cmd(uint8_t c) {
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, c);
}
static uint8_t ps2_mouse_read(void) {
    ps2_wait_read();
    return inb(0x60);
}
static void ps2_flush(void) {
    for (int i = 0; i < 128; i++) {
        if (!(inb(0x64) & 1)) break;
        inb(0x60);
    }
}

/* ======== INIT ======== */
void mouse_init(void) {
    ps2_flush();

    /* Aux port etkinlestir */
    ps2_wait_write();
    outb(0x64, 0xA8);

    /* Controller yapilandirma */
    ps2_wait_write();
    outb(0x64, 0x20);
    ps2_wait_read();
    uint8_t cfg = inb(0x60);
    cfg |= 0x02;
    cfg &= ~0x20;
    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, cfg);

    /* Reset mouse */
    ps2_mouse_cmd(0xFF);
    ps2_mouse_read();  /* ACK */
    ps2_mouse_read();  /* BAT (0xAA) */
    ps2_mouse_read();  /* ID  (0x00) */

    /* Defaults */
    ps2_mouse_cmd(0xF6);
    ps2_mouse_read();

    /* Enable data reporting */
    ps2_mouse_cmd(0xF4);
    ps2_mouse_read();

    ps2_flush();

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
    prev_left = 0;
    prev_right = 0;
    cur_left = 0;
    cur_right = 0;
    pending_click = 0;
    pending_rclick = 0;
    cycle = 0;
    ready = 1;
}

/* ======== POLL ======== */
void mouse_poll(mouse_state_t* state) {
    state->x = mx;
    state->y = my;
    state->left = 0;
    state->right = 0;
    state->middle = 0;
    state->click = 0;
    state->right_click = 0;

    if (!ready) return;

    uint8_t cur_l = 0;
    uint8_t cur_r = 0;
    int got_packet = 0;

    int bytes_read = 0;
    while (bytes_read < 12) {
        uint8_t status = inb(0x64);
        if (!(status & 0x01)) break;

        uint8_t byte = inb(0x60);
        bytes_read++;

        if (!(status & 0x20)) continue;

        if (cycle == 0) {
            if (!(byte & 0x08)) continue;
            if (byte & 0xC0) continue;
            pkt[0] = byte;
            cycle = 1;
        } else if (cycle == 1) {
            pkt[1] = byte;
            cycle = 2;
        } else {
            pkt[2] = byte;
            cycle = 0;
            got_packet = 1;

            int dx = (int)pkt[1];
            int dy = (int)pkt[2];
            if (pkt[0] & 0x10) dx -= 256;
            if (pkt[0] & 0x20) dy -= 256;
            if (dx > 50) dx = 50;
            if (dx < -50) dx = -50;
            if (dy > 50) dy = 50;
            if (dy < -50) dy = -50;

            mx += dx;
            my -= dy;
            if (mx < 0) mx = 0;
            if (mx >= sw) mx = sw - 1;
            if (my < 0) my = 0;
            if (my >= sh) my = sh - 1;

            cur_l = (pkt[0] & 0x01) ? 1 : 0;
            cur_r = (pkt[0] & 0x02) ? 1 : 0;
        }
    }

    state->x = mx;
    state->y = my;

    if (got_packet) {
        state->left = cur_l;
        state->right = cur_r;

        /* Click: SADECE 0→1 gecisi VE onceki frame'de 0 idi */
        if (cur_l && !prev_left) {
            state->click = 1;
        }
        if (cur_r && !prev_right) {
            state->right_click = 1;
        }

        prev_left = cur_l;
        prev_right = cur_r;
    } else {
        /* Paket gelmediyse onceki durumu koru AMA click uretme */
        state->left = prev_left;
        state->right = prev_right;
        state->click = 0;
        state->right_click = 0;
    }
}

/* ======== CURSOR ======== */
void mouse_draw_cursor(int cx, int cy) {
    static const uint8_t shape[15][13] = {
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
            if (shape[r][c] == 2)
                vesa_putpixel_raw(cx + c, cy + r, 0xFFFFFF);
            else if (shape[r][c] == 1)
                vesa_putpixel_raw(cx + c, cy + r, 0x000000);
        }
}
