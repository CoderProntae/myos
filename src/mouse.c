#include "mouse.h"
#include "io.h"

#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD    0x64

#define SCREEN_W 640
#define SCREEN_H 200

static int mouse_x = 320;
static int mouse_y = 100;
static int initialized = 0;

static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout--) {
        if (!(inb(MOUSE_STATUS) & 2)) return;
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(MOUSE_STATUS) & 1) return;
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(MOUSE_CMD, 0xD4);
    mouse_wait_write();
    outb(MOUSE_PORT, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_PORT);
}

void mouse_init(void) {
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);

    mouse_wait_write();
    outb(MOUSE_CMD, 0x20);
    mouse_wait_read();
    uint8_t status = inb(MOUSE_PORT);
    status |= 2;
    status &= (uint8_t)~0x20;
    mouse_wait_write();
    outb(MOUSE_CMD, 0x60);
    mouse_wait_write();
    outb(MOUSE_PORT, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    mouse_x = 320;
    mouse_y = 100;
    initialized = 1;
}

void mouse_poll(mouse_state_t* state) {
    if (!initialized) {
        state->x = mouse_x;
        state->y = mouse_y;
        state->col = mouse_x / 8;
        state->row = mouse_y / 8;
        state->left = 0;
        state->right = 0;
        state->middle = 0;
        return;
    }

    if (inb(MOUSE_STATUS) & 1) {
        uint8_t flags = inb(MOUSE_PORT);

        if (!(flags & 0x08)) {
            state->x = mouse_x;
            state->y = mouse_y;
            state->col = mouse_x / 8;
            state->row = mouse_y / 8;
            state->left = 0;
            state->right = 0;
            state->middle = 0;
            return;
        }

        mouse_wait_read();
        int8_t dx = (int8_t)inb(MOUSE_PORT);
        mouse_wait_read();
        int8_t dy = (int8_t)inb(MOUSE_PORT);

        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;

        state->left = flags & 0x01;
        state->right = flags & 0x02;
        state->middle = flags & 0x04;
    }

    state->x = mouse_x;
    state->y = mouse_y;
    state->col = mouse_x / 8;
    state->row = mouse_y / 8;
}
