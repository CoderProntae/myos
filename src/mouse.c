#include "mouse.h"
#include "io.h"

static mouse_state_t current = {40, 12, 0, 0, 0};
static uint8_t packet[3];
static int cycle = 0;
static int acc_x = 0;
static int acc_y = 0;

static void mouse_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if ((inb(0x64) & 2) == 0) return;
}

static void mouse_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
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

    mouse_write(0xF4);
    mouse_read();
}

void mouse_get_state(mouse_state_t* state) {
    if (state) *state = current;
}

int mouse_poll(mouse_state_t* state) {
    int changed = 0;

    while (1) {
        uint8_t st = inb(0x64);
        if (!(st & 1)) break;

        uint8_t data = inb(0x60);

        if (!(st & 0x20)) continue;

        if (cycle == 0 && !(data & 0x08)) continue;

        packet[cycle++] = data;
        if (cycle < 3) continue;
        cycle = 0;

        if (packet[0] & 0xC0) continue;

        int dx = (int)((int8_t)packet[1]);
        int dy = (int)((int8_t)packet[2]);

        acc_x += dx;
        acc_y -= dy;

        while (acc_x >= 4)  { current.x++; acc_x -= 4; }
        while (acc_x <= -4) { current.x--; acc_x += 4; }
        while (acc_y >= 4)  { current.y++; acc_y -= 4; }
        while (acc_y <= -4) { current.y--; acc_y += 4; }

        if (current.x < 0)  current.x = 0;
        if (current.x > 79) current.x = 79;
        if (current.y < 0)  current.y = 0;
        if (current.y > 24) current.y = 24;

        current.left   = (packet[0] & 0x01) ? 1 : 0;
        current.right  = (packet[0] & 0x02) ? 1 : 0;
        current.middle = (packet[0] & 0x04) ? 1 : 0;

        changed = 1;
    }

    if (changed && state) *state = current;
    return changed;
}
