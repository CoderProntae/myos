#include "mouse.h"
#include "io.h"

#define SCREEN_W 80
#define SCREEN_H 25
#define SCALE    4

static mouse_state_t current;
static uint8_t packet[3];
static int cycle = 0;
static int sub_x = 0;
static int sub_y = 0;

static void mouse_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

static void mouse_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(0x64) & 1) return;
    }
}

static void mouse_write(uint8_t value) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, value);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init(void) {
    current.x = SCREEN_W / 2;
    current.y = SCREEN_H / 2;
    current.left = 0;
    current.right = 0;
    current.middle = 0;
    sub_x = current.x * SCALE;
    sub_y = current.y * SCALE;

    mouse_wait_write();
    outb(0x64, 0xA8);

    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t status = inb(0x60);
    status |= 0x02;
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF3);
    mouse_read();
    mouse_write(100);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    cycle = 0;
}

void mouse_get_state(mouse_state_t* state) {
    if (state) *state = current;
}

static void flush_buffer(void) {
    for (int i = 0; i < 256; i++) {
        if (!(inb(0x64) & 1)) break;
        inb(0x60);
    }
}

int mouse_poll(mouse_state_t* state) {
    int changed = 0;
    int reads = 0;

    while (reads < 64) {
        uint8_t st = inb(0x64);
        if (!(st & 1)) break;

        uint8_t data = inb(0x60);
        reads++;

        if (!(st & 0x20)) continue;

        if (cycle == 0 && !(data & 0x08)) {
            cycle = 0;
            flush_buffer();
            continue;
        }

        packet[cycle++] = data;
        if (cycle < 3) continue;
        cycle = 0;

        if (packet[0] & 0xC0) continue;

        int dx = (int)((int8_t)packet[1]);
        int dy = (int)((int8_t)packet[2]);

        sub_x += dx;
        sub_y -= dy;

        if (sub_x < 0) sub_x = 0;
        if (sub_x > (SCREEN_W - 1) * SCALE) sub_x = (SCREEN_W - 1) * SCALE;
        if (sub_y < 0) sub_y = 0;
        if (sub_y > (SCREEN_H - 1) * SCALE) sub_y = (SCREEN_H - 1) * SCALE;

        int new_x = sub_x / SCALE;
        int new_y = sub_y / SCALE;

        if (new_x < 0) new_x = 0;
        if (new_x >= SCREEN_W) new_x = SCREEN_W - 1;
        if (new_y < 0) new_y = 0;
        if (new_y >= SCREEN_H) new_y = SCREEN_H - 1;

        uint8_t new_left   = (packet[0] & 0x01) ? 1 : 0;
        uint8_t new_right  = (packet[0] & 0x02) ? 1 : 0;
        uint8_t new_middle = (packet[0] & 0x04) ? 1 : 0;

        if (new_x != current.x || new_y != current.y ||
            new_left != current.left || new_right != current.right ||
            new_middle != current.middle) {
            current.x = new_x;
            current.y = new_y;
            current.left = new_left;
            current.right = new_right;
            current.middle = new_middle;
            changed = 1;
        }
    }

    if (changed && state) *state = current;
    return changed;
}
