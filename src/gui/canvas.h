#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include "gfx.h"

/* Cairo benzeri context tabanli 2D cizim API */

/* Anti-aliased cizim kalitesi */
#define CANVAS_QUALITY_FAST   0
#define CANVAS_QUALITY_GOOD   1
#define CANVAS_QUALITY_BEST   2

/* Cizgi birlestirme */
#define CANVAS_JOIN_MITER  0
#define CANVAS_JOIN_ROUND  1
#define CANVAS_JOIN_BEVEL  2

/* Cizgi ucu */
#define CANVAS_CAP_BUTT    0
#define CANVAS_CAP_ROUND   1
#define CANVAS_CAP_SQUARE  2

/* Fill kurali */
#define CANVAS_FILL_NONZERO  0
#define CANVAS_FILL_EVENODD  1

/* Operator */
#define CANVAS_OP_OVER     0
#define CANVAS_OP_SOURCE   1
#define CANVAS_OP_CLEAR    2
#define CANVAS_OP_ADD      3

/* Font stili */
#define CANVAS_FONT_NORMAL 0
#define CANVAS_FONT_BOLD   1
#define CANVAS_FONT_ITALIC 2

/* Gradient tipleri */
#define CANVAS_GRAD_LINEAR  0
#define CANVAS_GRAD_RADIAL  1

/* Path komutu */
#define PATH_MOVE    0
#define PATH_LINE    1
#define PATH_CURVE   2
#define PATH_CLOSE   3
#define PATH_ARC     4
#define PATH_MAX     256

typedef struct {
    int type;
    int x1, y1;
    int x2, y2;
    int x3, y3;
    int radius;
    int start_angle;
    int end_angle;
} path_cmd_t;

/* Gradient stop */
#define GRAD_MAX_STOPS 8

typedef struct {
    float offset;
    uint32_t color;
} grad_stop_t;

typedef struct {
    int type;
    int x1, y1, x2, y2;
    int cx, cy, radius;
    grad_stop_t stops[GRAD_MAX_STOPS];
    int stop_count;
} gradient_t;

/* Canvas context */
typedef struct {
    /* Hedef surface */
    uint32_t* pixels;
    int width;
    int height;

    /* Cizim durumu */
    uint32_t stroke_color;
    uint32_t fill_color;
    int      stroke_width;
    int      line_cap;
    int      line_join;
    int      fill_rule;
    int      op;
    uint8_t  global_alpha;
    int      quality;

    /* Font */
    int      font_size;
    int      font_style;

    /* Path */
    path_cmd_t path[PATH_MAX];
    int        path_count;
    int        path_x, path_y;

    /* Clip */
    int clip_x, clip_y, clip_w, clip_h;
    int clip_active;

    /* Transform (basit: translate only) */
    int translate_x;
    int translate_y;

    /* Gradient */
    gradient_t* active_gradient;

    /* Shadow */
    int      shadow_enable;
    int      shadow_x, shadow_y;
    int      shadow_blur;
    uint32_t shadow_color;

} canvas_t;

/* ======== Context ======== */
canvas_t*  canvas_create(int width, int height);
canvas_t*  canvas_create_for_screen(void);
void       canvas_destroy(canvas_t* ctx);

/* ======== Durum ======== */
void canvas_set_stroke_color(canvas_t* ctx, uint32_t color);
void canvas_set_fill_color(canvas_t* ctx, uint32_t color);
void canvas_set_stroke_width(canvas_t* ctx, int width);
void canvas_set_line_cap(canvas_t* ctx, int cap);
void canvas_set_line_join(canvas_t* ctx, int join);
void canvas_set_alpha(canvas_t* ctx, uint8_t alpha);
void canvas_set_operator(canvas_t* ctx, int op);
void canvas_set_quality(canvas_t* ctx, int quality);
void canvas_set_font_size(canvas_t* ctx, int size);
void canvas_set_font_style(canvas_t* ctx, int style);

/* ======== Transform ======== */
void canvas_translate(canvas_t* ctx, int dx, int dy);
void canvas_reset_transform(canvas_t* ctx);

/* ======== Clip ======== */
void canvas_clip(canvas_t* ctx, int x, int y, int w, int h);
void canvas_reset_clip(canvas_t* ctx);

/* ======== Shadow ======== */
void canvas_set_shadow(canvas_t* ctx, int ox, int oy, int blur, uint32_t color);
void canvas_disable_shadow(canvas_t* ctx);

/* ======== Path ======== */
void canvas_begin_path(canvas_t* ctx);
void canvas_move_to(canvas_t* ctx, int x, int y);
void canvas_line_to(canvas_t* ctx, int x, int y);
void canvas_close_path(canvas_t* ctx);
void canvas_arc(canvas_t* ctx, int cx, int cy, int r, int start_deg, int end_deg);
void canvas_rect_path(canvas_t* ctx, int x, int y, int w, int h);
void canvas_rounded_rect_path(canvas_t* ctx, int x, int y, int w, int h, int r);
void canvas_stroke(canvas_t* ctx);
void canvas_fill(canvas_t* ctx);

/* ======== Hizli cizim (path'siz) ======== */
void canvas_clear(canvas_t* ctx, uint32_t color);
void canvas_pixel(canvas_t* ctx, int x, int y, uint32_t color);
void canvas_line(canvas_t* ctx, int x1, int y1, int x2, int y2);
void canvas_rect(canvas_t* ctx, int x, int y, int w, int h);
void canvas_fill_rect(canvas_t* ctx, int x, int y, int w, int h);
void canvas_stroke_rect(canvas_t* ctx, int x, int y, int w, int h);
void canvas_circle(canvas_t* ctx, int cx, int cy, int r);
void canvas_fill_circle(canvas_t* ctx, int cx, int cy, int r);
void canvas_stroke_circle(canvas_t* ctx, int cx, int cy, int r);
void canvas_rounded_rect(canvas_t* ctx, int x, int y, int w, int h, int r);
void canvas_fill_rounded_rect(canvas_t* ctx, int x, int y, int w, int h, int r);

/* ======== Gelismis sekiller ======== */
void canvas_ellipse(canvas_t* ctx, int cx, int cy, int rx, int ry);
void canvas_fill_ellipse(canvas_t* ctx, int cx, int cy, int rx, int ry);
void canvas_polygon(canvas_t* ctx, const int* points, int count);
void canvas_fill_polygon(canvas_t* ctx, const int* points, int count);
void canvas_star(canvas_t* ctx, int cx, int cy, int outer_r, int inner_r, int points);
void canvas_fill_star(canvas_t* ctx, int cx, int cy, int outer_r, int inner_r, int points);

/* ======== Gradient ======== */
gradient_t* canvas_create_linear_gradient(int x1, int y1, int x2, int y2);
gradient_t* canvas_create_radial_gradient(int cx, int cy, int r);
void canvas_gradient_add_stop(gradient_t* g, float offset, uint32_t color);
void canvas_set_fill_gradient(canvas_t* ctx, gradient_t* g);
void canvas_free_gradient(gradient_t* g);

/* ======== Metin ======== */
void canvas_text(canvas_t* ctx, int x, int y, const char* text);
void canvas_fill_text(canvas_t* ctx, int x, int y, const char* text);
int  canvas_measure_text(canvas_t* ctx, const char* text);

/* ======== Goruntu islemleri ======== */
void canvas_blit(canvas_t* ctx, canvas_t* src, int x, int y);
void canvas_blit_rect(canvas_t* ctx, canvas_t* src,
                       int sx, int sy, int sw, int sh,
                       int dx, int dy);
void canvas_blit_scaled(canvas_t* ctx, canvas_t* src,
                         int dx, int dy, int dw, int dh);

/* ======== Filtreler ======== */
void canvas_blur(canvas_t* ctx, int radius);
void canvas_brightness(canvas_t* ctx, int amount);
void canvas_grayscale(canvas_t* ctx);
void canvas_invert(canvas_t* ctx);

/* ======== Flush ======== */
void canvas_flush(canvas_t* ctx);

#endif
