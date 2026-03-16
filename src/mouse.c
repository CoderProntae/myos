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

/* Click latch - tiklandiginda 1 olur, okunana kadar kalir */
static uint8_t click_pending = 0;
static uint8_t rclick_pending = 0;
static uint8_t cur_left = 0;
static uint8_t cur_right = 0;
static uint8_t cur_middle = 0;

static void wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}

static void wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}

static void mcmd(uint8_t c) {
    wait_write();
    outb(0x64, 0xD4);
    wait_write();
    outb(0x60, c);
}

static uint8_t mread(void) {
    wait_read();
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

    wait_write(); outb(0x64, 0xA8);

    wait_write(); outb(0x64, 0x20);
    wait_read();
    uint8_t status = inb(0x60);
    status |= 0x02;
    status &= ~0x20;
    wait_write(); outb(0x64, 0x60);
    wait_write(); outb(0x60, status);

    mcmd(0xFF);
    mread(); mread(); mread();

    mcmd(0xF6); mread();
    mcmd(0xF4); mread();

    flush();

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
    prev_left = 0;
    prev_right = 0;
    cycle = 0;
    click_pending = 0;
    rclick_pending = 0;
    ready = 1;
}

void mouse_clear_click(void) {
    click_pending = 0;
    rclick_pending = 0;
}

void mouse_poll(mouse_state_t* state) {
    if (!ready) {
        state->x = mx; state->y = my;
        state->left = state->right = state->middle = 0;
        state->click = 0; state->right_click = 0;
        return;
    }

    /* Bekleyen paketleri isle */
    int reads = 0;
    while (reads < 30) {
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) break;

        if (!(st & 0x20)) {
            /* Klavye verisi - atla */
            inb(0x60);
            reads++;
            continue;
        }

        uint8_t b = inb(0x60);
        reads++;

        if (cycle == 0) {
            if (!(b & 0x08)) continue;
            if (b & 0xC0) continue;
            pkt[0] = b;
            cycle = 1;
        } else if (cycle == 1) {
            pkt[1] = b;
            cycle = 2;
        } else {
            pkt[2] = b;
            cycle = 0;

            int dx = (int)pkt[1];
            int dy = (int)pkt[2];
            if (pkt[0] & 0x10) dx -= 256;
            if (pkt[0] & 0x20) dy -= 256;

            mx += dx;
            my -= dy;

            if (mx < 0) mx = 0;
            if (mx >= sw) mx = sw - 1;
            if (my < 0) my = 0;
            if (my >= sh) my = sh - 1;

            cur_left   = (pkt[0] & 0x01) ? 1 : 0;
            cur_right  = (pkt[0] & 0x02) ? 1 : 0;
            cur_middle = (pkt[0] & 0x04) ? 1 : 0;

            /* Click latch: basildigi an latch'le, ta ki okunana kadar */
            if (cur_left && !prev_left) click_pending = 1;
            if (cur_right && !prev_right) rclick_pending = 1;

            prev_left = cur_left;
            prev_right = cur_right;
        }
    }

    state->x = mx;
    state->y = my;
    state->left = cur_left;
    state->right = cur_right;
    state->middle = cur_middle;
    state->click = click_pending;
    state->right_click = rclick_pending;
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
                vesa_putpixel_raw(cx+c, cy+r, 0xFFFFFF);
            else if (cur[r][c] == 1)
                vesa_putpixel_raw(cx+c, cy+r, 0x000000);
        }
}
