#include "gfx.h"
#include "vesa.h"
#include "heap.h"
#include "string.h"
#include "font.h"

static int clip_x = 0, clip_y = 0;
static int clip_w = 800, clip_h = 600;
static int clip_active = 0;

/* ======== Init ======== */

int gfx_init(void) {
    clip_x = 0; clip_y = 0;
    clip_w = vesa_get_width();
    clip_h = vesa_get_height();
    clip_active = 0;
    return 0;
}

void gfx_quit(void) {}

int gfx_get_width(void) { return vesa_get_width(); }
int gfx_get_height(void) { return vesa_get_height(); }

/* ======== Clip ======== */

void gfx_set_clip(int x, int y, int w, int h) {
    clip_x = x; clip_y = y; clip_w = w; clip_h = h;
    clip_active = 1;
}

void gfx_reset_clip(void) {
    clip_x = 0; clip_y = 0;
    clip_w = vesa_get_width();
    clip_h = vesa_get_height();
    clip_active = 0;
}

static int clip_check(int x, int y) {
    if (!clip_active) return 1;
    return (x >= clip_x && x < clip_x + clip_w &&
            y >= clip_y && y < clip_y + clip_h);
}

/* ======== Temel Cizim ======== */

void gfx_clear(uint32_t color) {
    vesa_fill_screen(color);
}

void gfx_pixel(int x, int y, uint32_t color) {
    if (clip_check(x, y))
        vesa_putpixel(x, y, color);
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    vesa_fill_rect(x, y, w, h, color);
}

void gfx_rect(int x, int y, int w, int h, uint32_t color) {
    vesa_draw_rect_outline(x, y, w, h, color);
}

/* ======== Line (Bresenham) ======== */

void gfx_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int err = dx - dy;

    while (1) {
        gfx_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

/* ======== Circle (Midpoint) ======== */

void gfx_circle(int cx, int cy, int r, uint32_t color) {
    int x = r, y = 0;
    int d = 1 - r;

    while (x >= y) {
        gfx_pixel(cx+x, cy+y, color); gfx_pixel(cx-x, cy+y, color);
        gfx_pixel(cx+x, cy-y, color); gfx_pixel(cx-x, cy-y, color);
        gfx_pixel(cx+y, cy+x, color); gfx_pixel(cx-y, cy+x, color);
        gfx_pixel(cx+y, cy-x, color); gfx_pixel(cx-y, cy-x, color);
        y++;
        if (d <= 0) { d += 2*y + 1; }
        else { x--; d += 2*(y-x) + 1; }
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r)
                gfx_pixel(cx+dx, cy+dy, color);
        }
    }
}

/* ======== Triangle ======== */

void gfx_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    gfx_line(x1, y1, x2, y2, color);
    gfx_line(x2, y2, x3, y3, color);
    gfx_line(x3, y3, x1, y1, color);
}

void gfx_fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    /* Basit scanline fill */
    int min_y = y1 < y2 ? (y1 < y3 ? y1 : y3) : (y2 < y3 ? y2 : y3);
    int max_y = y1 > y2 ? (y1 > y3 ? y1 : y3) : (y2 > y3 ? y2 : y3);

    for (int y = min_y; y <= max_y; y++) {
        int min_x = vesa_get_width(), max_x = 0;

        /* Her kenarla kesisimi bul */
        int edges[3][4] = {{x1,y1,x2,y2},{x2,y2,x3,y3},{x3,y3,x1,y1}};
        for (int e = 0; e < 3; e++) {
            int ex1 = edges[e][0], ey1 = edges[e][1];
            int ex2 = edges[e][2], ey2 = edges[e][3];
            if ((ey1 <= y && ey2 > y) || (ey2 <= y && ey1 > y)) {
                int ix = ex1 + (y - ey1) * (ex2 - ex1) / (ey2 - ey1);
                if (ix < min_x) min_x = ix;
                if (ix > max_x) max_x = ix;
            }
        }

        if (min_x <= max_x) {
            for (int x = min_x; x <= max_x; x++)
                gfx_pixel(x, y, color);
        }
    }
}

/* ======== Rounded Rect ======== */

void gfx_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    gfx_fill_rect(x + r, y, w - 2*r, h, color);
    gfx_fill_rect(x, y + r, r, h - 2*r, color);
    gfx_fill_rect(x + w - r, y + r, r, h - 2*r, color);

    for (int cy2 = 0; cy2 < r; cy2++) {
        for (int cx2 = 0; cx2 < r; cx2++) {
            if (cx2*cx2 + cy2*cy2 <= r*r) {
                gfx_pixel(x + r - cx2, y + r - cy2, color);
                gfx_pixel(x + w - r + cx2 - 1, y + r - cy2, color);
                gfx_pixel(x + r - cx2, y + h - r + cy2 - 1, color);
                gfx_pixel(x + w - r + cx2 - 1, y + h - r + cy2 - 1, color);
            }
        }
    }
}

/* ======== Alpha Blending ======== */

uint32_t gfx_blend(uint32_t bg, uint32_t fg, uint8_t alpha) {
    uint32_t br = GFX_R(bg), bg2 = GFX_G(bg), bb = GFX_B(bg);
    uint32_t fr = GFX_R(fg), fg2 = GFX_G(fg), fb = GFX_B(fg);
    uint32_t a = alpha, ia = 255 - alpha;

    uint32_t r = (fr * a + br * ia) / 255;
    uint32_t g = (fg2 * a + bg2 * ia) / 255;
    uint32_t b = (fb * a + bb * ia) / 255;

    return GFX_RGB(r, g, b);
}

void gfx_fill_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    int sw = vesa_get_width(), sh = vesa_get_height();
    for (int py = y; py < y + h && py < sh; py++) {
        if (py < 0) continue;
        for (int px = x; px < x + w && px < sw; px++) {
            if (px < 0) continue;
            uint32_t bg = backbuffer[py * sw + px];
            backbuffer[py * sw + px] = gfx_blend(bg, color, alpha);
        }
    }
}

/* ======== Gradient ======== */

void gfx_gradient_h(int x, int y, int w, int h, uint32_t c1, uint32_t c2) {
    for (int px = 0; px < w; px++) {
        uint32_t r = GFX_R(c1) + (GFX_R(c2) - GFX_R(c1)) * px / w;
        uint32_t g = GFX_G(c1) + (GFX_G(c2) - GFX_G(c1)) * px / w;
        uint32_t b = GFX_B(c1) + (GFX_B(c2) - GFX_B(c1)) * px / w;
        uint32_t color = GFX_RGB(r, g, b);
        for (int py = y; py < y + h; py++)
            gfx_pixel(x + px, py, color);
    }
}

void gfx_gradient_v(int x, int y, int w, int h, uint32_t c1, uint32_t c2) {
    for (int py = 0; py < h; py++) {
        uint32_t r = GFX_R(c1) + (GFX_R(c2) - GFX_R(c1)) * py / h;
        uint32_t g = GFX_G(c1) + (GFX_G(c2) - GFX_G(c1)) * py / h;
        uint32_t b = GFX_B(c1) + (GFX_B(c2) - GFX_B(c1)) * py / h;
        uint32_t color = GFX_RGB(r, g, b);
        for (int px = x; px < x + w; px++)
            gfx_pixel(px, y + py, color);
    }
}

/* ======== Metin ======== */

void gfx_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    vesa_draw_char(x, y, c, fg, bg);
}

void gfx_text(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    vesa_draw_string(x, y, str, fg, bg);
}

void gfx_text_nobg(int x, int y, const char* str, uint32_t fg) {
    vesa_draw_string_nobg(x, y, str, fg);
}

int gfx_text_width(const char* str) {
    return k_strlen(str) * 8;
}

/* ======== Surface ======== */

gfx_surface_t* gfx_create_surface(int w, int h) {
    gfx_surface_t* s = (gfx_surface_t*)kmalloc(sizeof(gfx_surface_t));
    if (!s) return 0;
    s->width = w;
    s->height = h;
    s->pitch = w;
    s->pixels = (uint32_t*)kmalloc((uint32_t)(w * h * 4));
    if (!s->pixels) { kfree(s); return 0; }
    k_memset(s->pixels, 0, (uint32_t)(w * h * 4));
    return s;
}

void gfx_free_surface(gfx_surface_t* s) {
    if (!s) return;
    if (s->pixels) kfree(s->pixels);
    kfree(s);
}

void gfx_blit(gfx_surface_t* src, const gfx_rect_t* srcrect,
              int dst_x, int dst_y) {
    if (!src || !src->pixels) return;
    int sx = srcrect ? srcrect->x : 0;
    int sy = srcrect ? srcrect->y : 0;
    int sw2 = srcrect ? srcrect->w : src->width;
    int sh2 = srcrect ? srcrect->h : src->height;

    for (int py = 0; py < sh2; py++) {
        for (int px = 0; px < sw2; px++) {
            int src_idx = (sy + py) * src->pitch + (sx + px);
            if (src_idx >= 0 && src_idx < src->width * src->height) {
                uint32_t c = src->pixels[src_idx];
                if (c != 0) /* seffaf piksel atlama */
                    gfx_pixel(dst_x + px, dst_y + py, c);
            }
        }
    }
}

void gfx_blit_scaled(gfx_surface_t* src, int dst_x, int dst_y,
                      int dst_w, int dst_h) {
    if (!src || !src->pixels) return;

    for (int py = 0; py < dst_h; py++) {
        int sy = (py * src->height) / dst_h;
        for (int px = 0; px < dst_w; px++) {
            int sx = (px * src->width) / dst_w;
            int idx = sy * src->pitch + sx;
            if (idx >= 0 && idx < src->width * src->height) {
                uint32_t c = src->pixels[idx];
                if (c != 0)
                    gfx_pixel(dst_x + px, dst_y + py, c);
            }
        }
    }
}

/* ======== Flip ======== */

void gfx_flip(void) {
    vesa_copy_buffer();
}
