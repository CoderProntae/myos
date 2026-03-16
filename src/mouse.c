#include "mouse.h"
#include "vesa.h"
#include "io.h"

/* ======== Durum ======== */
static int mx, my;
static int sw = 800, sh = 600;
static int ready = 0;

/* Paket biriktirme */
static int    cycle = 0;
static uint8_t pkt[3];

/* Buton gecmisi */
static uint8_t last_left  = 0;
static uint8_t last_right = 0;

/* Click latch: set edilir, mouse_poll donus degerine kopyalandiktan
   sonra AYNI FRAME ICINDE temizlenir. Boylece hic kaybolmaz. */
static uint8_t latch_click  = 0;
static uint8_t latch_rclick = 0;

/* ======== Dusuk seviye ======== */
static void w_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}
static void w_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}
static void m_cmd(uint8_t c) {
    w_write(); outb(0x64, 0xD4);
    w_write(); outb(0x60, c);
}
static uint8_t m_read(void) {
    w_read();
    return inb(0x60);
}
static void m_flush(void) {
    for (int i = 0; i < 128; i++) {
        if (!(inb(0x64) & 1)) break;
        inb(0x60);
    }
}

/* ======== Init ======== */
void mouse_init(void) {
    m_flush();

    /* Aux port ac */
    w_write(); outb(0x64, 0xA8);

    /* Controller yapilandirma */
    w_write(); outb(0x64, 0x20);
    w_read();
    uint8_t cfg = inb(0x60);
    cfg |= 0x02;       /* IRQ12 */
    cfg &= ~0x20;      /* Clock enable */
    w_write(); outb(0x64, 0x60);
    w_write(); outb(0x60, cfg);

    /* Mouse reset */
    m_cmd(0xFF);
    m_read(); /* ACK  */
    m_read(); /* 0xAA */
    m_read(); /* 0x00 */

    /* Varsayilanlara don */
    m_cmd(0xF6);
    m_read();

    /* Veri akisini baslat */
    m_cmd(0xF4);
    m_read();

    m_flush();

    sw = vesa_get_width();
    sh = vesa_get_height();
    mx = sw / 2;
    my = sh / 2;
    last_left = 0;
    last_right = 0;
    latch_click = 0;
    latch_rclick = 0;
    cycle = 0;
    ready = 1;
}

/* ======== Tek paket isle ======== */
static void process_packet(void) {
    /*
     * PS/2 Mouse paket formati:
     *   pkt[0]: Y_OVF X_OVF Y_SIGN X_SIGN 1 MID RIGHT LEFT
     *   pkt[1]: X delta (unsigned, sign bit pkt[0] bit4)
     *   pkt[2]: Y delta (unsigned, sign bit pkt[0] bit5)
     *
     * Sign extension:
     *   pkt[1] unsigned 0-255.
     *   Eger pkt[0] bit4 set ise, gercek deger = pkt[1] - 256
     *   Aksi halde gercek deger = pkt[1]
     */
    int dx = (int)pkt[1];
    int dy = (int)pkt[2];

    if (pkt[0] & 0x10) dx -= 256;   /* X sign extend */
    if (pkt[0] & 0x20) dy -= 256;   /* Y sign extend */

    /* Sapma korumasi */
    if (dx > 80)  dx = 80;
    if (dx < -80) dx = -80;
    if (dy > 80)  dy = 80;
    if (dy < -80) dy = -80;

    /* Pozisyon guncelle */
    mx += dx;
    my -= dy;  /* PS/2'de Y yukari pozitif, ekranda asagi pozitif */

    if (mx < 0)   mx = 0;
    if (mx >= sw)  mx = sw - 1;
    if (my < 0)   my = 0;
    if (my >= sh)  my = sh - 1;

    /* Buton durumu */
    uint8_t cur_left  = (pkt[0] & 0x01) ? 1 : 0;
    uint8_t cur_right = (pkt[0] & 0x02) ? 1 : 0;

    /* Edge detection: 0->1 gecisinde latch set et */
    if (cur_left && !last_left)   latch_click = 1;
    if (cur_right && !last_right) latch_rclick = 1;

    last_left  = cur_left;
    last_right = cur_right;
}

/* ======== Poll ======== */
void mouse_poll(mouse_state_t* state) {
    /* Onceki frame'den kalan latch'leri state'e kopyala */
    state->click       = latch_click;
    state->right_click = latch_rclick;

    /* Latch'leri SIMDI temizle — bu frame'de tekrar set olabilir */
    latch_click  = 0;
    latch_rclick = 0;

    if (!ready) {
        state->x = mx;
        state->y = my;
        state->left = state->right = state->middle = 0;
        return;
    }

    /*
     * Bekleyen mouse bytelerini oku.
     * En fazla 4 paket (12 byte) isle — daha fazlasi lag yapar.
     */
    int max_bytes = 12;
    int count = 0;

    while (count < max_bytes) {
        uint8_t status = inb(0x64);

        /* Veri yok */
        if (!(status & 0x01))
            break;

        uint8_t byte = inb(0x60);
        count++;

        /* Bit 5: 0=klavye, 1=mouse */
        if (!(status & 0x20))
            continue;   /* Klavye verisi — atla */

        /* Mouse byte'ini paket state machine'e ver */
        switch (cycle) {
        case 0:
            /* Byte 0: flags */
            if (!(byte & 0x08)) {
                /* Bit 3 set degil = senkronizasyon kaybi, atla */
                continue;
            }
            if (byte & 0xC0) {
                /* Overflow = bozuk paket, atla */
                continue;
            }
            pkt[0] = byte;
            cycle = 1;
            break;

        case 1:
            /* Byte 1: X delta */
            pkt[1] = byte;
            cycle = 2;
            break;

        case 2:
            /* Byte 2: Y delta — paket tamam */
            pkt[2] = byte;
            cycle = 0;
            process_packet();
            break;
        }
    }

    /* Cikis degerlerini ayarla */
    state->x      = mx;
    state->y      = my;
    state->left   = last_left;
    state->right  = last_right;
    state->middle = 0;  /* middle butonu su an kullanilmiyor */

    /* Bu frame'de click oldu mu? (process_packet icinde set edilmis olabilir) */
    if (latch_click)  state->click = 1;
    if (latch_rclick) state->right_click = 1;

    /* Latch'leri TEKRAR temizle — sonraki frame'e tasinmasin */
    latch_click  = 0;
    latch_rclick = 0;
}

/* ======== Cursor cizim ======== */
void mouse_draw_cursor(int cx, int cy) {
    /*
     * 15x13 ok isareti
     * 0 = seffaf, 1 = siyah kenar, 2 = beyaz ic
     */
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
