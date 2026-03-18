#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEY_F1    0x0100
#define KEY_F2    0x0101
#define KEY_F3    0x0102
#define KEY_F4    0x0103
#define KEY_F5    0x0104
#define KEY_F6    0x0105
#define KEY_F7    0x0106
#define KEY_F8    0x0107
#define KEY_F9    0x0108
#define KEY_F10   0x0109
#define KEY_F11   0x010A
#define KEY_F12   0x010B
#define KEY_UP    0x0110
#define KEY_DOWN  0x0111
#define KEY_LEFT  0x0112
#define KEY_RIGHT 0x0113
#define KEY_ESC   0x0114

int  keyboard_getchar(void);
int  keyboard_poll(void);
int  keyboard_alt_held(void);
int  keyboard_ctrl_held(void);

#endif
