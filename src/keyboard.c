#include "keyboard.h"
#include "io.h"

static const char scancode_ascii[] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

static const char scancode_shift[] = {
    0, 27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
};

static int shift_pressed = 0;

char keyboard_getchar(void) {
    uint8_t sc;
    while (1) {
        while (!(inb(0x64) & 1));
        sc = inb(0x60);
        if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; continue; }
        if (sc & 0x80) continue;
        if (sc < sizeof(scancode_ascii)) {
            char c = shift_pressed ? scancode_shift[sc] : scancode_ascii[sc];
            if (c) return c;
        }
    }
}
