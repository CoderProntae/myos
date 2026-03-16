#include "keyboard.h"
#include "io.h"

static const char scancode_ascii[] = {
    0, 0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};
static const char scancode_shift[] = {
    0, 0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
};

static int shift_held = 0;
static int alt_held = 0;
static int ctrl_held = 0;

int keyboard_alt_held(void)  { return alt_held; }
int keyboard_ctrl_held(void) { return ctrl_held; }

static int decode_scancode(uint8_t sc) {
    /* Modifier tuslar */
    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift_held = 0; return 0; }
    if (sc == 0x38) { alt_held = 1; return 0; }
    if (sc == 0xB8) { alt_held = 0; return 0; }
    if (sc == 0x1D) { ctrl_held = 1; return 0; }
    if (sc == 0x9D) { ctrl_held = 0; return 0; }

    /* Tus birakma */
    if (sc & 0x80) return 0;

    /* Ozel tuslar */
    if (sc == 0x01) return KEY_ESC;
    if (sc >= 0x3B && sc <= 0x44) return KEY_F1 + (sc - 0x3B);
    if (sc == 0x57) return KEY_F11;
    if (sc == 0x58) return KEY_F12;
    if (sc == 0x48) return KEY_UP;
    if (sc == 0x50) return KEY_DOWN;
    if (sc == 0x4B) return KEY_LEFT;
    if (sc == 0x4D) return KEY_RIGHT;

    /* Normal tuslar */
    if (sc < sizeof(scancode_ascii)) {
        char c = shift_held ? scancode_shift[sc] : scancode_ascii[sc];
        if (c) return (int)c;
    }
    return 0;
}

int keyboard_poll(void) {
    uint8_t status = inb(0x64);
    if (!(status & 1)) return 0;
    if (status & 0x20) { inb(0x60); return 0; }
    uint8_t sc = inb(0x60);
    return decode_scancode(sc);
}

int keyboard_getchar(void) {
    while (1) {
        int c = keyboard_poll();
        if (c) return c;
    }
}
