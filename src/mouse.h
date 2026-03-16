#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x, y;
    uint8_t left, right, middle;
    uint8_t click;
    uint8_t right_click;
} mouse_state_t;

void mouse_init(void);
void mouse_poll(mouse_state_t* state);
void mouse_draw_cursor(int x, int y);
void mouse_clear_click(void);

#endif
