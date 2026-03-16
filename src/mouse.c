#include "mouse.h"
#include "vesa.h"
#include "io.h"

static int mx = 400, my = 300;
static int sw = 800, sh = 600;
static uint8_t prev_left = 0;

static void mouse_wait_w(void) {
    int t = 100000;
    while (t--) { if (!(inb(0x64) & 2)) return; }
}

static void mouse_wait_r(void) {
    int t = 100000;
    while (t--) { if (inb(0x64) & 1) return; }
}

static void mouse_cmd(uint8_t cmd) {
    mouse_wait_w(); outb(0x64, 0xD4);
    mouse_wait_w(); outb(0x60, cmd);
}

void mouse_init(void) {
    mouse_wait_w(); outb(0x64, 0xA8);

    mouse_wait_w(); outb(0x64, 0x20);
    mouse_wait_r();
    uint8_t status = inb(0x60);
    status |= 2;
    status &= (uint8_t)~0x20;
    mouse_wait_w(); outb(0x64, 0x60);
    mouse_wait_w(); outb(0x60, status);

    mouse_cmd(0xF6); mouse_wait_r(); inb(0x60);
    mouse_cmd(0xF4); mouse_wait_r(); inb(0x60);

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
}

void mouse_poll(mouse_state_t* state) {
    state->click = 0;

    if (inb(0x64) & 1) {
        uint8_t flags = inb(0x60);
        if (flags & 0x08) {
            mouse_wait_r(); int8_t dx = (int8_t)inb(0x60);
            mouse_wait_r(); int8_t dy = (int8_t)inb(0x60);

            mx += dx * 2;  /* hiz carpani */
            my -= dy * 2;

            if (mx < 0) mx = 0;
            if (mx >= sw) mx = sw - 1;
            if (my < 0) my = 0;
            if (my >= sh) my = sh - 1;

            state->left   = flags & 0x01;
            state->right  = flags & 0x02;
            state->middle = flags & 0x04;

            /* Click event: basildigi an */
            if (state->left && !prev_left) state->click = 1;
            prev_left = state->left;
        }
    }

    state->x = mx;
    state->y = my;
}

/* Modern ok isareti cursor */
void mouse_draw_cursor(int cx, int cy) {
    /* 12x16 ok isareti */
    static const char* shape[] = {
        "X...........",
        "XX..........",
        "XXX.........",
        "XXXX........",
        "XXXXX.......",
        "XXXXXX......",
        "XXXXXXX.....",
        "XXXXXXXX....",
        "XXXXXXXXX...",
        "XXXXXXXXXX..",
        "XXXXXXX.....",
        "XXX.XXX.....",
        "XX..XXX.....",
        "X....XXX....",
        ".....XXX....",
        "......X.....",
    };

    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 12; c++) {
            if (shape[r][c] == 'X')
                vesa_putpixel(cx + c, cy + r, COLOR_TEXT_WHITE);
        }
    }
}
