#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x;
    int y;
    uint8_t left;
    uint8_t right;
    uint8_t middle;
} mouse_state_t;

void mouse_init(void);
int mouse_poll(mouse_state_t* state);
void mouse_get_state(mouse_state_t* state);

#endif
