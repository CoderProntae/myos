#ifndef GFX_H
#define GFX_H

#include <stdint.h>

/* SDL benzeri basit grafik API */

typedef struct {
    int x, y, w, h;
} gfx_rect_t;

typedef struct {
    uint8_t r, g, b, a;
} gfx_color_t;

typedef struct {
    uint32_t* pixels;
    int width;
    int height;
    int pitch;
} gfx_surface_t;

/* Renk makrolari */
#define GFX_RGB(r,g,b) (((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b))
#define GFX_RGBA(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b))
#define GFX_R(c) (((c)>>16)&0xFF)
#define GFX_G(c) (((c)>>8)&0xFF)
#define GFX_B(c) ((c)&0xFF)
#define GFX_A(c) (((c)>>24)&0xFF)

/* Temel renkler */
#define GFX_BLACK   0x000000
#define GFX_WHITE   0xFFFFFF
#define GFX_RED     0xFF0000
#define GFX_GREEN   0x00FF00
#define GFX_BLUE    0x0000FF
#define GFX_YELLOW  0xFFFF00
#define GFX_CYAN    0x00FFFF
#define GFX_MAGENTA 0xFF00FF
#define GFX_GREY    0x808080
#define GFX_DARK    0x1E1E2E

/* Init/Quit */
int  gfx_init(void);
void gfx_quit(void);
int  gfx_get_width(void);
int  gfx_get_height(void);

/* Cizim */
void gfx_clear(uint32_t color);
void gfx_pixel(int x, int y, uint32_t color);
void gfx_line(int x1, int y1, int x2, int y2, uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_circle(int cx, int cy, int r, uint32_t color);
void gfx_fill_circle(int cx, int cy, int r, uint32_t color);
void gfx_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void gfx_fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void gfx_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);

/* Metin */
void gfx_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void gfx_text(int x, int y, const char* str, uint32_t fg, uint32_t bg);
void gfx_text_nobg(int x, int y, const char* str, uint32_t fg);
int  gfx_text_width(const char* str);

/* Surface */
gfx_surface_t* gfx_create_surface(int w, int h);
void gfx_free_surface(gfx_surface_t* s);
void gfx_blit(gfx_surface_t* src, const gfx_rect_t* srcrect,
              int dst_x, int dst_y);
void gfx_blit_scaled(gfx_surface_t* src, int dst_x, int dst_y,
                      int dst_w, int dst_h);

/* Alpha blending */
uint32_t gfx_blend(uint32_t bg, uint32_t fg, uint8_t alpha);
void gfx_fill_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);

/* Gradient */
void gfx_gradient_h(int x, int y, int w, int h, uint32_t c1, uint32_t c2);
void gfx_gradient_v(int x, int y, int w, int h, uint32_t c1, uint32_t c2);

/* Flush */
void gfx_flip(void);

/* Clipboard */
void gfx_set_clip(int x, int y, int w, int h);
void gfx_reset_clip(void);

#endif
