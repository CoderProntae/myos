#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x;
    int y;
    int col;
    int row;
    uint8_t left;
    uint8_t right;
    uint8_t middle;
} mouse_state_t;

void mouse_init(void);
void mouse_poll(mouse_state_t* state);

#endif
