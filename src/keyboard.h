#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEY_F1    0x80
#define KEY_F2    0x81
#define KEY_F3    0x82
#define KEY_F4    0x83
#define KEY_F5    0x84
#define KEY_F6    0x85
#define KEY_F7    0x86
#define KEY_F8    0x87
#define KEY_F9    0x88
#define KEY_F10   0x89
#define KEY_F11   0x8A
#define KEY_F12   0x8B
#define KEY_UP    0x90
#define KEY_DOWN  0x91
#define KEY_LEFT  0x92
#define KEY_RIGHT 0x93
#define KEY_ESC   0x94

char keyboard_getchar(void);
char keyboard_poll(void);          /* bloklamayan okuma */
int  keyboard_alt_held(void);
int  keyboard_ctrl_held(void);

#endif
