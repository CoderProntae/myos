#include "mouse.h"
#include "vesa.h"
#include "io.h"

static int mx, my;
static int sw = 800, sh = 600;
static uint8_t prev_left = 0;
static int inited = 0;

/* Byte bekle/oku */
static void mwait_w(void) { int t=100000; while(t--) if(!(inb(0x64)&2)) return; }
static void mwait_r(void) { int t=100000; while(t--) if(inb(0x64)&1) return; }
static void mcmd(uint8_t c) { mwait_w(); outb(0x64,0xD4); mwait_w(); outb(0x60,c); }

/* Mouse buffer'ini bosalt */
static void flush_mouse(void) {
    int t = 1000;
    while (t-- > 0 && (inb(0x64) & 1))
        inb(0x60);
}

void mouse_init(void) {
    flush_mouse();

    mwait_w(); outb(0x64, 0xA8);            /* Aux port etkinlestir */
    mwait_w(); outb(0x64, 0x20);            /* Durum oku */
    mwait_r(); uint8_t st = inb(0x60);
    st |= 2; st &= (uint8_t)~0x20;         /* IRQ12 + etkinlestir */
    mwait_w(); outb(0x64, 0x60);
    mwait_w(); outb(0x60, st);

    mcmd(0xF6); mwait_r(); inb(0x60);       /* Varsayilanlara sifirla */
    mcmd(0xF4); mwait_r(); inb(0x60);       /* Veri akisini baslat */

    /* Ornekleme hizini artir: 200 samples/sec */
    mcmd(0xF3); mwait_r(); inb(0x60);
    mcmd(200);  mwait_r(); inb(0x60);

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
    prev_left = 0;
    inited = 1;
}

void mouse_poll(mouse_state_t* state) {
    state->click = 0;
    state->left = 0;
    state->right = 0;
    state->middle = 0;

    if (!inited) { state->x=mx; state->y=my; return; }

    /* Birden fazla paket varsa hepsini isleyelim */
    int packets = 0;
    while ((inb(0x64) & 0x21) == 0x21 && packets < 5) {
        uint8_t flags = inb(0x60);
        if (!(flags & 0x08)) continue; /* Senkronizasyon hatasi */

        mwait_r(); int8_t dx = (int8_t)inb(0x60);
        mwait_r(); int8_t dy = (int8_t)inb(0x60);

        /* Ivme: hizli hareket = daha buyuk adim */
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;
        int accel = 1;
        if (adx > 5 || ady > 5) accel = 2;
        if (adx > 15 || ady > 15) accel = 3;

        mx += dx * accel;
        my -= dy * accel;

        if (mx < 0) mx = 0;
        if (mx >= sw) mx = sw - 1;
        if (my < 0) my = 0;
        if (my >= sh) my = sh - 1;

        state->left   = (flags & 0x01) ? 1 : 0;
        state->right  = (flags & 0x02) ? 1 : 0;
        state->middle = (flags & 0x04) ? 1 : 0;

        if (state->left && !prev_left) state->click = 1;
        prev_left = state->left;
        packets++;
    }

    state->x = mx;
    state->y = my;
}

void mouse_draw_cursor(int cx, int cy) {
    static const char* shape[] = {
        "Xo..........",
        "XXo.........",
        "XXXo........",
        "XXXXo.......",
        "XXXXXo......",
        "XXXXXXo.....",
        "XXXXXXXo....",
        "XXXXXXXXo...",
        "XXXXXXXXXo..",
        "XXXXXXooooo.",
        "XXXoXXo.....",
        "XXo.oXXo....",
        "Xo...oXXo...",
        "o.....oXXo..",
        ".......oo...",
    };
    for (int r=0;r<15;r++)
        for (int c=0;c<13;c++) {
            if (shape[r][c]=='X')
                vesa_putpixel_raw(cx+c, cy+r, 0xFFFFFF);
            else if (shape[r][c]=='o')
                vesa_putpixel_raw(cx+c, cy+r, 0x000000);
        }
}
