#include "canvas.h"
#include "vesa.h"
#include "heap.h"
#include "string.h"
#include "font.h"

/* ======== Yardimci ======== */

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int iabs(int x) { return x < 0 ? -x : x; }

static uint32_t blend_pixel(uint32_t bg, uint32_t fg, uint8_t alpha) {
    if (alpha == 255) return fg;
    if (alpha == 0) return bg;
    uint32_t br = (bg >> 16) & 0xFF, bg2 = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    uint32_t fr = (fg >> 16) & 0xFF, fg2 = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    uint32_t a = alpha, ia = 255 - alpha;
    uint32_t r = (fr * a + br * ia) / 255;
    uint32_t g = (fg2 * a + bg2 * ia) / 255;
    uint32_t b = (fb * a + bb * ia) / 255;
    return (r << 16) | (g << 8) | b;
}

/* Sin/cos tablo (derece -> 256 olcekli) */
/* sin(x) * 256 */
static const int16_t sin_table[91] = {
    0,4,9,13,18,22,27,31,36,40,44,49,53,57,62,66,70,74,79,83,
    87,91,95,99,103,107,111,115,119,122,126,130,133,137,140,143,
    147,150,153,156,159,162,165,167,170,173,175,178,180,182,184,
    187,189,191,193,194,196,198,199,201,202,204,205,206,207,208,
    209,210,211,212,213,214,214,215,215,216,216,217,217,217,217,
    218,218,218,218,218,218,218,218,218,218,256
};

static int fast_sin(int deg) {
    deg = deg % 360;
    if (deg < 0) deg += 360;
    if (deg <= 90) return sin_table[deg];
    if (deg <= 180) return sin_table[180 - deg];
    if (deg <= 270) return -sin_table[deg - 180];
    return -sin_table[360 - deg];
}

static int fast_cos(int deg) {
    return fast_sin(deg + 90);
}

/* ======== Context ======== */

canvas_t* canvas_create(int width, int height) {
    canvas_t* ctx = (canvas_t*)kmalloc(sizeof(canvas_t));
    if (!ctx) return 0;
    k_memset(ctx, 0, sizeof(canvas_t));

    ctx->pixels = (uint32_t*)kmalloc((uint32_t)(width * height * 4));
    if (!ctx->pixels) { kfree(ctx); return 0; }
    k_memset(ctx->pixels, 0, (uint32_t)(width * height * 4));

    ctx->width = width;
    ctx->height = height;
    ctx->stroke_color = 0xFFFFFF;
    ctx->fill_color = 0xFFFFFF;
    ctx->stroke_width = 1;
    ctx->global_alpha = 255;
    ctx->font_size = 16;
    ctx->quality = CANVAS_QUALITY_GOOD;
    ctx->clip_w = width;
    ctx->clip_h = height;
    return ctx;
}

canvas_t* canvas_create_for_screen(void) {
    canvas_t* ctx = (canvas_t*)kmalloc(sizeof(canvas_t));
    if (!ctx) return 0;
    k_memset(ctx, 0, sizeof(canvas_t));

    ctx->pixels = backbuffer;
    ctx->width = vesa_get_width();
    ctx->height = vesa_get_height();
    ctx->stroke_color = 0xFFFFFF;
    ctx->fill_color = 0xFFFFFF;
    ctx->stroke_width = 1;
    ctx->global_alpha = 255;
    ctx->font_size = 16;
    ctx->quality = CANVAS_QUALITY_GOOD;
    ctx->clip_w = ctx->width;
    ctx->clip_h = ctx->height;
    return ctx;
}

void canvas_destroy(canvas_t* ctx) {
    if (!ctx) return;
    if (ctx->pixels && ctx->pixels != backbuffer) kfree(ctx->pixels);
    kfree(ctx);
}

/* ======== Durum ======== */

void canvas_set_stroke_color(canvas_t* ctx, uint32_t c) { ctx->stroke_color = c; }
void canvas_set_fill_color(canvas_t* ctx, uint32_t c) { ctx->fill_color = c; }
void canvas_set_stroke_width(canvas_t* ctx, int w) { ctx->stroke_width = w; }
void canvas_set_line_cap(canvas_t* ctx, int c) { ctx->line_cap = c; }
void canvas_set_line_join(canvas_t* ctx, int j) { ctx->line_join = j; }
void canvas_set_alpha(canvas_t* ctx, uint8_t a) { ctx->global_alpha = a; }
void canvas_set_operator(canvas_t* ctx, int o) { ctx->op = o; }
void canvas_set_quality(canvas_t* ctx, int q) { ctx->quality = q; }
void canvas_set_font_size(canvas_t* ctx, int s) { ctx->font_size = s; }
void canvas_set_font_style(canvas_t* ctx, int s) { ctx->font_style = s; }

void canvas_translate(canvas_t* ctx, int dx, int dy) { ctx->translate_x += dx; ctx->translate_y += dy; }
void canvas_reset_transform(canvas_t* ctx) { ctx->translate_x = 0; ctx->translate_y = 0; }

void canvas_clip(canvas_t* ctx, int x, int y, int w, int h) {
    ctx->clip_x = x; ctx->clip_y = y; ctx->clip_w = w; ctx->clip_h = h; ctx->clip_active = 1;
}
void canvas_reset_clip(canvas_t* ctx) {
    ctx->clip_x = 0; ctx->clip_y = 0; ctx->clip_w = ctx->width; ctx->clip_h = ctx->height; ctx->clip_active = 0;
}

void canvas_set_shadow(canvas_t* ctx, int ox, int oy, int blur, uint32_t color) {
    ctx->shadow_enable = 1; ctx->shadow_x = ox; ctx->shadow_y = oy; ctx->shadow_blur = blur; ctx->shadow_color = color;
}
void canvas_disable_shadow(canvas_t* ctx) { ctx->shadow_enable = 0; }

/* ======== Pixel ======== */

static void put_pixel(canvas_t* ctx, int x, int y, uint32_t color) {
    x += ctx->translate_x;
    y += ctx->translate_y;

    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) return;
    if (ctx->clip_active) {
        if (x < ctx->clip_x || x >= ctx->clip_x + ctx->clip_w ||
            y < ctx->clip_y || y >= ctx->clip_y + ctx->clip_h) return;
    }

    if (ctx->global_alpha == 255) {
        ctx->pixels[y * ctx->width + x] = color;
    } else {
        uint32_t bg = ctx->pixels[y * ctx->width + x];
        ctx->pixels[y * ctx->width + x] = blend_pixel(bg, color, ctx->global_alpha);
    }
}

void canvas_pixel(canvas_t* ctx, int x, int y, uint32_t color) {
    put_pixel(ctx, x, y, color);
}

void canvas_clear(canvas_t* ctx, uint32_t color) {
    for (int i = 0; i < ctx->width * ctx->height; i++)
        ctx->pixels[i] = color;
}

/* ======== Line ======== */

static void thick_pixel(canvas_t* ctx, int x, int y, uint32_t color, int thickness) {
    int half = thickness / 2;
    for (int dy = -half; dy <= half; dy++)
        for (int dx = -half; dx <= half; dx++)
            put_pixel(ctx, x + dx, y + dy, color);
}

void canvas_line(canvas_t* ctx, int x1, int y1, int x2, int y2) {
    int dx = iabs(x2 - x1), dy = iabs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int w = ctx->stroke_width;
    uint32_t c = ctx->stroke_color;

    while (1) {
        if (w > 1) thick_pixel(ctx, x1, y1, c, w);
        else put_pixel(ctx, x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

/* ======== Rect ======== */

void canvas_fill_rect(canvas_t* ctx, int x, int y, int w, int h) {
    /* Shadow */
    if (ctx->shadow_enable) {
        for (int py = 0; py < h; py++)
            for (int px = 0; px < w; px++) {
                int sx = x + px + ctx->shadow_x;
                int sy = y + py + ctx->shadow_y;
                uint32_t bg = 0;
                if (sx >= 0 && sx < ctx->width && sy >= 0 && sy < ctx->height)
                    bg = ctx->pixels[(sy + ctx->translate_y) * ctx->width + sx + ctx->translate_x];
                uint32_t sc = blend_pixel(bg, ctx->shadow_color, 80);
                put_pixel(ctx, sx, sy, sc);
            }
    }

    uint32_t c = ctx->fill_color;
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            put_pixel(ctx, px, py, c);
}

void canvas_stroke_rect(canvas_t* ctx, int x, int y, int w, int h) {
    int sw = ctx->stroke_width;
    uint32_t c = ctx->stroke_color;
    for (int i = 0; i < sw; i++) {
        for (int px = x; px < x + w; px++) { put_pixel(ctx, px, y + i, c); put_pixel(ctx, px, y + h - 1 - i, c); }
        for (int py = y; py < y + h; py++) { put_pixel(ctx, x + i, py, c); put_pixel(ctx, x + w - 1 - i, py, c); }
    }
}

void canvas_rect(canvas_t* ctx, int x, int y, int w, int h) {
    canvas_fill_rect(ctx, x, y, w, h);
    canvas_stroke_rect(ctx, x, y, w, h);
}

/* ======== Rounded Rect ======== */

void canvas_fill_rounded_rect(canvas_t* ctx, int x, int y, int w, int h, int r) {
    uint32_t c = ctx->fill_color;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    /* Orta alan */
    for (int py = y + r; py < y + h - r; py++)
        for (int px = x; px < x + w; px++)
            put_pixel(ctx, px, py, c);
    /* Ust/alt serit */
    for (int py = y; py < y + r; py++)
        for (int px = x + r; px < x + w - r; px++)
            put_pixel(ctx, px, py, c);
    for (int py = y + h - r; py < y + h; py++)
        for (int px = x + r; px < x + w - r; px++)
            put_pixel(ctx, px, py, c);
    /* Koseler */
    for (int cy2 = 0; cy2 < r; cy2++)
        for (int cx2 = 0; cx2 < r; cx2++)
            if (cx2 * cx2 + cy2 * cy2 <= r * r) {
                put_pixel(ctx, x + r - cx2, y + r - cy2, c);
                put_pixel(ctx, x + w - r + cx2 - 1, y + r - cy2, c);
                put_pixel(ctx, x + r - cx2, y + h - r + cy2 - 1, c);
                put_pixel(ctx, x + w - r + cx2 - 1, y + h - r + cy2 - 1, c);
            }
}

void canvas_rounded_rect(canvas_t* ctx, int x, int y, int w, int h, int r) {
    canvas_fill_rounded_rect(ctx, x, y, w, h, r);
}

/* ======== Circle ======== */

void canvas_fill_circle(canvas_t* ctx, int cx, int cy, int r) {
    uint32_t c = ctx->fill_color;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r)
                put_pixel(ctx, cx + dx, cy + dy, c);
}

void canvas_stroke_circle(canvas_t* ctx, int cx, int cy, int r) {
    int x = r, y = 0, d = 1 - r;
    uint32_t c = ctx->stroke_color;
    while (x >= y) {
        thick_pixel(ctx, cx+x, cy+y, c, ctx->stroke_width);
        thick_pixel(ctx, cx-x, cy+y, c, ctx->stroke_width);
        thick_pixel(ctx, cx+x, cy-y, c, ctx->stroke_width);
        thick_pixel(ctx, cx-x, cy-y, c, ctx->stroke_width);
        thick_pixel(ctx, cx+y, cy+x, c, ctx->stroke_width);
        thick_pixel(ctx, cx-y, cy+x, c, ctx->stroke_width);
        thick_pixel(ctx, cx+y, cy-x, c, ctx->stroke_width);
        thick_pixel(ctx, cx-y, cy-x, c, ctx->stroke_width);
        y++;
        if (d <= 0) d += 2 * y + 1;
        else { x--; d += 2 * (y - x) + 1; }
    }
}

void canvas_circle(canvas_t* ctx, int cx, int cy, int r) {
    canvas_fill_circle(ctx, cx, cy, r);
    canvas_stroke_circle(ctx, cx, cy, r);
}

/* ======== Ellipse ======== */

void canvas_fill_ellipse(canvas_t* ctx, int cx, int cy, int rx, int ry) {
    uint32_t c = ctx->fill_color;
    for (int dy = -ry; dy <= ry; dy++)
        for (int dx = -rx; dx <= rx; dx++)
            if ((dx * dx * ry * ry + dy * dy * rx * rx) <= rx * rx * ry * ry)
                put_pixel(ctx, cx + dx, cy + dy, c);
}

void canvas_ellipse(canvas_t* ctx, int cx, int cy, int rx, int ry) {
    canvas_fill_ellipse(ctx, cx, cy, rx, ry);
}

/* ======== Polygon ======== */

void canvas_polygon(canvas_t* ctx, const int* pts, int count) {
    for (int i = 0; i < count; i++) {
        int ni = (i + 1) % count;
        int old_sw = ctx->stroke_width;
        canvas_line(ctx, pts[i*2], pts[i*2+1], pts[ni*2], pts[ni*2+1]);
        ctx->stroke_width = old_sw;
    }
}

void canvas_fill_polygon(canvas_t* ctx, const int* pts, int count) {
    /* Scanline fill */
    int min_y = ctx->height, max_y = 0;
    for (int i = 0; i < count; i++) {
        if (pts[i*2+1] < min_y) min_y = pts[i*2+1];
        if (pts[i*2+1] > max_y) max_y = pts[i*2+1];
    }

    for (int y = min_y; y <= max_y; y++) {
        int nodes[32], nc = 0;
        for (int i = 0; i < count && nc < 30; i++) {
            int ni = (i + 1) % count;
            int y1 = pts[i*2+1], y2 = pts[ni*2+1];
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                int x1 = pts[i*2], x2 = pts[ni*2];
                nodes[nc++] = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
            }
        }
        /* Sort */
        for (int i = 0; i < nc - 1; i++)
            for (int j = 0; j < nc - i - 1; j++)
                if (nodes[j] > nodes[j+1]) { int t = nodes[j]; nodes[j] = nodes[j+1]; nodes[j+1] = t; }
        /* Fill */
        for (int i = 0; i < nc - 1; i += 2)
            for (int x = nodes[i]; x <= nodes[i+1]; x++)
                put_pixel(ctx, x, y, ctx->fill_color);
    }
}

/* ======== Star ======== */

void canvas_fill_star(canvas_t* ctx, int cx, int cy, int outer_r, int inner_r, int n) {
    int pts[40]; /* max 20 nokta */
    int total = n * 2;
    if (total > 20) total = 20;

    for (int i = 0; i < total; i++) {
        int angle = (i * 360) / total - 90;
        int r = (i % 2 == 0) ? outer_r : inner_r;
        pts[i*2] = cx + (r * fast_cos(angle)) / 256;
        pts[i*2+1] = cy + (r * fast_sin(angle)) / 256;
    }
    canvas_fill_polygon(ctx, pts, total);
}

void canvas_star(canvas_t* ctx, int cx, int cy, int outer_r, int inner_r, int n) {
    canvas_fill_star(ctx, cx, cy, outer_r, inner_r, n);
}

/* ======== Gradient ======== */

gradient_t* canvas_create_linear_gradient(int x1, int y1, int x2, int y2) {
    gradient_t* g = (gradient_t*)kmalloc(sizeof(gradient_t));
    if (!g) return 0;
    k_memset(g, 0, sizeof(gradient_t));
    g->type = CANVAS_GRAD_LINEAR;
    g->x1 = x1; g->y1 = y1; g->x2 = x2; g->y2 = y2;
    return g;
}

gradient_t* canvas_create_radial_gradient(int cx, int cy, int r) {
    gradient_t* g = (gradient_t*)kmalloc(sizeof(gradient_t));
    if (!g) return 0;
    k_memset(g, 0, sizeof(gradient_t));
    g->type = CANVAS_GRAD_RADIAL;
    g->cx = cx; g->cy = cy; g->radius = r;
    return g;
}

void canvas_gradient_add_stop(gradient_t* g, float offset, uint32_t color) {
    if (!g || g->stop_count >= GRAD_MAX_STOPS) return;
    g->stops[g->stop_count].offset = offset;
    g->stops[g->stop_count].color = color;
    g->stop_count++;
}

void canvas_set_fill_gradient(canvas_t* ctx, gradient_t* g) {
    ctx->active_gradient = g;
}

void canvas_free_gradient(gradient_t* g) {
    if (g) kfree(g);
}

static uint32_t gradient_color_at(gradient_t* g, int x, int y) {
    if (!g || g->stop_count < 2) return 0xFFFFFF;

    int t_256; /* 0-256 arasi interpolasyon */

    if (g->type == CANVAS_GRAD_LINEAR) {
        int dx = g->x2 - g->x1, dy = g->y2 - g->y1;
        int len2 = dx * dx + dy * dy;
        if (len2 == 0) return g->stops[0].color;
        int dot = (x - g->x1) * dx + (y - g->y1) * dy;
        t_256 = (dot * 256) / len2;
    } else {
        int dx = x - g->cx, dy = y - g->cy;
        int dist = 0;
        /* Basit sqrt yaklasimi */
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        dist = (dx > dy) ? dx + dy / 2 : dy + dx / 2;
        t_256 = (g->radius > 0) ? (dist * 256) / g->radius : 0;
    }

    if (t_256 < 0) t_256 = 0;
    if (t_256 > 256) t_256 = 256;

    /* Stop'lar arasi interpolasyon */
    for (int i = 0; i < g->stop_count - 1; i++) {
        int s1 = (int)(g->stops[i].offset * 256);
        int s2 = (int)(g->stops[i+1].offset * 256);
        if (t_256 >= s1 && t_256 <= s2) {
            int range = s2 - s1;
            if (range == 0) return g->stops[i].color;
            int local_t = ((t_256 - s1) * 255) / range;
            return blend_pixel(g->stops[i].color, g->stops[i+1].color, (uint8_t)local_t);
        }
    }
    return g->stops[g->stop_count - 1].color;
}

/* ======== Path ======== */

void canvas_begin_path(canvas_t* ctx) { ctx->path_count = 0; }

void canvas_move_to(canvas_t* ctx, int x, int y) {
    if (ctx->path_count >= PATH_MAX) return;
    ctx->path[ctx->path_count].type = PATH_MOVE;
    ctx->path[ctx->path_count].x1 = x;
    ctx->path[ctx->path_count].y1 = y;
    ctx->path_count++;
    ctx->path_x = x; ctx->path_y = y;
}

void canvas_line_to(canvas_t* ctx, int x, int y) {
    if (ctx->path_count >= PATH_MAX) return;
    ctx->path[ctx->path_count].type = PATH_LINE;
    ctx->path[ctx->path_count].x1 = ctx->path_x;
    ctx->path[ctx->path_count].y1 = ctx->path_y;
    ctx->path[ctx->path_count].x2 = x;
    ctx->path[ctx->path_count].y2 = y;
    ctx->path_count++;
    ctx->path_x = x; ctx->path_y = y;
}

void canvas_close_path(canvas_t* ctx) {
    if (ctx->path_count >= PATH_MAX || ctx->path_count == 0) return;
    /* Ilk MOVE noktasina geri don */
    for (int i = ctx->path_count - 1; i >= 0; i--) {
        if (ctx->path[i].type == PATH_MOVE) {
            canvas_line_to(ctx, ctx->path[i].x1, ctx->path[i].y1);
            break;
        }
    }
}

void canvas_stroke(canvas_t* ctx) {
    for (int i = 0; i < ctx->path_count; i++) {
        if (ctx->path[i].type == PATH_LINE) {
            int x1 = ctx->path[i].x1, y1 = ctx->path[i].y1;
            int x2 = ctx->path[i].x2, y2 = ctx->path[i].y2;
            canvas_line(ctx, x1, y1, x2, y2);
        }
    }
}

void canvas_fill(canvas_t* ctx) {
    /* Path noktalarindan polygon olustur */
    int pts[PATH_MAX * 2];
    int pc = 0;
    for (int i = 0; i < ctx->path_count; i++) {
        if (ctx->path[i].type == PATH_MOVE) {
            pts[pc*2] = ctx->path[i].x1;
            pts[pc*2+1] = ctx->path[i].y1;
            pc++;
        } else if (ctx->path[i].type == PATH_LINE) {
            pts[pc*2] = ctx->path[i].x2;
            pts[pc*2+1] = ctx->path[i].y2;
            pc++;
        }
    }
    if (pc >= 3) canvas_fill_polygon(ctx, pts, pc);
}

/* ======== Metin ======== */

void canvas_fill_text(canvas_t* ctx, int x, int y, const char* text) {
    x += ctx->translate_x;
    y += ctx->translate_y;
    uint32_t fg = ctx->fill_color;

    while (*text) {
        if (*text >= FONT_FIRST && *text <= FONT_LAST) {
            const uint8_t* glyph = font_data[*text - FONT_FIRST];
            for (int row = 0; row < FONT_HEIGHT; row++) {
                uint8_t bits = glyph[row];
                for (int col = 0; col < FONT_WIDTH; col++) {
                    if (bits & (0x80 >> col)) {
                        int px2 = x + col, py2 = y + row;
                        if (px2 >= 0 && px2 < ctx->width && py2 >= 0 && py2 < ctx->height) {
                            if (ctx->global_alpha == 255)
                                ctx->pixels[py2 * ctx->width + px2] = fg;
                            else
                                ctx->pixels[py2 * ctx->width + px2] =
                                    blend_pixel(ctx->pixels[py2 * ctx->width + px2], fg, ctx->global_alpha);
                        }
                        /* Bold */
                        if (ctx->font_style & CANVAS_FONT_BOLD) {
                            int bx = px2 + 1;
                            if (bx >= 0 && bx < ctx->width && py2 >= 0 && py2 < ctx->height)
                                ctx->pixels[py2 * ctx->width + bx] = fg;
                        }
                    }
                }
            }
        }
        x += FONT_WIDTH;
        text++;
    }
}

void canvas_text(canvas_t* ctx, int x, int y, const char* text) {
    canvas_fill_text(ctx, x, y, text);
}

int canvas_measure_text(canvas_t* ctx, const char* text) {
    (void)ctx;
    return k_strlen(text) * FONT_WIDTH;
}

/* ======== Blit ======== */

void canvas_blit(canvas_t* ctx, canvas_t* src, int x, int y) {
    if (!src) return;
    for (int py = 0; py < src->height; py++)
        for (int px = 0; px < src->width; px++) {
            uint32_t c = src->pixels[py * src->width + px];
            if (c) put_pixel(ctx, x + px, y + py, c);
        }
}

void canvas_blit_rect(canvas_t* ctx, canvas_t* src,
                       int sx, int sy, int sw, int sh,
                       int dx, int dy) {
    if (!src) return;
    for (int py = 0; py < sh; py++)
        for (int px = 0; px < sw; px++) {
            int si = (sy + py) * src->width + (sx + px);
            if (si >= 0 && si < src->width * src->height) {
                uint32_t c = src->pixels[si];
                if (c) put_pixel(ctx, dx + px, dy + py, c);
            }
        }
}

void canvas_blit_scaled(canvas_t* ctx, canvas_t* src, int dx, int dy, int dw, int dh) {
    if (!src) return;
    for (int py = 0; py < dh; py++)
        for (int px = 0; px < dw; px++) {
            int sx = (px * src->width) / dw;
            int sy = (py * src->height) / dh;
            int si = sy * src->width + sx;
            if (si >= 0 && si < src->width * src->height) {
                uint32_t c = src->pixels[si];
                if (c) put_pixel(ctx, dx + px, dy + py, c);
            }
        }
}

/* ======== Filtreler ======== */

void canvas_blur(canvas_t* ctx, int radius) {
    if (radius <= 0) return;
    uint32_t* tmp = (uint32_t*)kmalloc((uint32_t)(ctx->width * ctx->height * 4));
    if (!tmp) return;

    /* Basit box blur */
    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            uint32_t r = 0, g = 0, b = 0, count = 0;
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < ctx->width && ny >= 0 && ny < ctx->height) {
                        uint32_t c = ctx->pixels[ny * ctx->width + nx];
                        r += (c >> 16) & 0xFF;
                        g += (c >> 8) & 0xFF;
                        b += c & 0xFF;
                        count++;
                    }
                }
            }
            if (count > 0)
                tmp[y * ctx->width + x] = ((r/count)<<16)|((g/count)<<8)|(b/count);
        }
    }

    for (int i = 0; i < ctx->width * ctx->height; i++)
        ctx->pixels[i] = tmp[i];
    kfree(tmp);
}

void canvas_brightness(canvas_t* ctx, int amount) {
    for (int i = 0; i < ctx->width * ctx->height; i++) {
        uint32_t c = ctx->pixels[i];
        int r = clamp(((c>>16)&0xFF) + amount, 0, 255);
        int g = clamp(((c>>8)&0xFF) + amount, 0, 255);
        int b = clamp((c&0xFF) + amount, 0, 255);
        ctx->pixels[i] = (r << 16) | (g << 8) | b;
    }
}

void canvas_grayscale(canvas_t* ctx) {
    for (int i = 0; i < ctx->width * ctx->height; i++) {
        uint32_t c = ctx->pixels[i];
        int gray = (((c>>16)&0xFF)*30 + ((c>>8)&0xFF)*59 + (c&0xFF)*11) / 100;
        ctx->pixels[i] = (gray << 16) | (gray << 8) | gray;
    }
}

void canvas_invert(canvas_t* ctx) {
    for (int i = 0; i < ctx->width * ctx->height; i++) {
        uint32_t c = ctx->pixels[i];
        ctx->pixels[i] = ((255-((c>>16)&0xFF))<<16)|((255-((c>>8)&0xFF))<<8)|(255-(c&0xFF));
    }
}

/* ======== Flush ======== */

void canvas_flush(canvas_t* ctx) {
    if (ctx->pixels == backbuffer)
        vesa_copy_buffer();
}

/* ======== Path helper ======== */

void canvas_arc(canvas_t* ctx, int cx, int cy, int r, int start_deg, int end_deg) {
    int prev_x = cx + (r * fast_cos(start_deg)) / 256;
    int prev_y = cy + (r * fast_sin(start_deg)) / 256;

    if (ctx->path_count == 0)
        canvas_move_to(ctx, prev_x, prev_y);

    for (int deg = start_deg + 5; deg <= end_deg; deg += 5) {
        int nx = cx + (r * fast_cos(deg)) / 256;
        int ny = cy + (r * fast_sin(deg)) / 256;
        canvas_line_to(ctx, nx, ny);
    }
}

void canvas_rect_path(canvas_t* ctx, int x, int y, int w, int h) {
    canvas_move_to(ctx, x, y);
    canvas_line_to(ctx, x + w, y);
    canvas_line_to(ctx, x + w, y + h);
    canvas_line_to(ctx, x, y + h);
    canvas_close_path(ctx);
}

void canvas_rounded_rect_path(canvas_t* ctx, int x, int y, int w, int h, int r) {
    canvas_move_to(ctx, x + r, y);
    canvas_line_to(ctx, x + w - r, y);
    canvas_arc(ctx, x + w - r, y + r, r, 270, 360);
    canvas_line_to(ctx, x + w, y + h - r);
    canvas_arc(ctx, x + w - r, y + h - r, r, 0, 90);
    canvas_line_to(ctx, x + r, y + h);
    canvas_arc(ctx, x + r, y + h - r, r, 90, 180);
    canvas_line_to(ctx, x, y + r);
    canvas_arc(ctx, x + r, y + r, r, 180, 270);
}
