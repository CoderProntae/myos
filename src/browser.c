#include "browser.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"
#include "http.h"
#include "dns.h"
#include "net.h"

#define ADDR_MAX 128
#define PAGE_MAX 8192
#define HIST_MAX 8

/* Renkler */
#define BR_BG         0x1B1B2F
#define BR_TAB_BG     0x252542
#define BR_TAB_ACTIVE 0x1B1B2F
#define BR_NAV_BG     0x20203A
#define BR_ADDR_BG    0x15152A
#define BR_ADDR_FOCUS 0x101025
#define BR_ADDR_BORDER 0x0078D4
#define BR_CONTENT_BG 0x1B1B2F
#define BR_STATUS_BG  0x18182E
#define BR_BOOKMARK_BG 0x1E1E36
#define BR_BTN_HOVER  0x3A3A5A
#define BR_LINK_COLOR 0x4499FF
#define BR_TEXT        0xDDDDDD
#define BR_TEXT_DIM    0x888899
#define BR_HEADING1    0x66DDFF
#define BR_HEADING2    0x55CC55
#define BR_BOLD        0xFFFFFF
#define BR_ERROR       0xFF6666
#define BR_SUCCESS     0x66FF66
#define BR_LOADING     0xFFCC44

static char addr_buf[ADDR_MAX];
static int  addr_len = 0;
static int  addr_editing = 0;
static int  scroll_y = 0;
static int  max_scroll = 0;
static int  loading = 0;

/* Sayfa */
static char page_title[64];
static char page_body[PAGE_MAX];
static int  page_body_len = 0;
static int  page_status = 0;
static char page_error[64];
static int  is_html = 0;

/* Gecmis */
static char history[HIST_MAX][ADDR_MAX];
static int  hist_pos = -1;
static int  hist_count = 0;

/* Yer imleri */
static const char* bm_names[] = {
    "Example", "CERN", "NeverSSL", "HTTPForever"
};
static const char* bm_urls[] = {
    "example.com", "info.cern.ch", "neverssl.com", "httpforever.com"
};
#define BM_COUNT 4

/* ======== Gecmis ======== */

static void hist_push(const char* url) {
    if (hist_pos >= 0 && k_strcmp(history[hist_pos], url) == 0) return;
    hist_pos++;
    if (hist_pos >= HIST_MAX) {
        for (int i = 0; i < HIST_MAX - 1; i++)
            k_strcpy(history[i], history[i + 1]);
        hist_pos = HIST_MAX - 1;
    }
    k_strcpy(history[hist_pos], url);
    hist_count = hist_pos + 1;
}

static void go_back(void) {
    if (hist_pos > 0) {
        hist_pos--;
        k_strcpy(addr_buf, history[hist_pos]);
        addr_len = k_strlen(addr_buf);
    }
}

static void go_forward(void) {
    if (hist_pos < hist_count - 1) {
        hist_pos++;
        k_strcpy(addr_buf, history[hist_pos]);
        addr_len = k_strlen(addr_buf);
    }
}

/* ======== HTML Tag kontrol ======== */

static int is_tag(const char* p, const char* tag) {
    if (*p != '<') return 0;
    p++;
    int tl = k_strlen(tag);
    for (int i = 0; i < tl; i++) {
        char a = p[i], b = tag[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
    }
    char n = p[tl];
    return (n == '>' || n == ' ' || n == '/' || n == '\r' || n == '\n');
}

static int skip_tag(const char* p) {
    int i = 0;
    while (p[i] && p[i] != '>') i++;
    if (p[i] == '>') i++;
    return i;
}

/* ======== Sayfa Render ======== */

static void render_page(int cy, int ch) {
    int sw = vesa_get_width();
    int margin = 40;
    int max_x = sw - margin;
    int x = margin, y = cy + 12 - scroll_y;
    int last_y = cy;

    if (!is_html) {
        /* Duz metin */
        const char* p = page_body;
        while (*p) {
            if (y > cy + ch) break;
            if (*p == '\n') {
                x = margin;
                y += 16;
            } else if (*p == '\r') {
                /* atla */
            } else if (*p >= 32 && *p < 127) {
                if (x >= max_x) { x = margin; y += 16; }
                if (y >= cy && y < cy + ch)
                    vesa_draw_char(x, y, *p, BR_TEXT, BR_CONTENT_BG);
                x += 8;
            }
            if (y > last_y) last_y = y;
            p++;
        }
        max_scroll = (last_y - cy + 50);
        if (max_scroll < 0) max_scroll = 0;
        return;
    }

    /* HTML render */
    const char* p = page_body;
    uint32_t color = BR_TEXT;
    int bold = 0;
    int in_title_tag = 0;
    int heading = 0;
    int skip_content = 0;
    int in_pre = 0;

    while (*p) {
        if (y > cy + ch + scroll_y) break;

        if (*p == '<') {
            /* Heading */
            if (is_tag(p, "h1")) {
                heading = 1; color = BR_HEADING1; x = margin; y += 12;
                p += skip_tag(p); continue;
            }
            if (is_tag(p, "h2")) {
                heading = 2; color = BR_HEADING2; x = margin; y += 10;
                p += skip_tag(p); continue;
            }
            if (is_tag(p, "h3")) {
                heading = 3; color = 0x44BBBB; x = margin; y += 8;
                p += skip_tag(p); continue;
            }
            if (is_tag(p, "/h1") || is_tag(p, "/h2") || is_tag(p, "/h3")) {
                heading = 0; color = BR_TEXT;
                y += heading == 1 ? 28 : 22;
                x = margin;
                /* Alt cizgi (h1 icin) */
                if (y >= cy && y < cy + ch) {
                    for (int hx = margin; hx < max_x; hx++)
                        vesa_putpixel(hx, y - 4, 0x333355);
                }
                p += skip_tag(p); continue;
            }

            /* Bold */
            if (is_tag(p, "b") || is_tag(p, "strong")) {
                bold = 1; color = BR_BOLD; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/b") || is_tag(p, "/strong")) {
                bold = 0; color = BR_TEXT; p += skip_tag(p); continue;
            }

            /* Italic/em — renk degistir */
            if (is_tag(p, "i") || is_tag(p, "em")) {
                color = 0xBBBBDD; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/i") || is_tag(p, "/em")) {
                color = BR_TEXT; p += skip_tag(p); continue;
            }

            /* Paragraf */
            if (is_tag(p, "p")) {
                x = margin; y += 18; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/p")) {
                y += 10; x = margin; p += skip_tag(p); continue;
            }

            /* Div */
            if (is_tag(p, "div")) {
                x = margin; y += 4; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/div")) {
                y += 4; x = margin; p += skip_tag(p); continue;
            }

            /* BR */
            if (is_tag(p, "br")) {
                x = margin; y += 16; p += skip_tag(p); continue;
            }

            /* Link */
            if (is_tag(p, "a")) {
                color = BR_LINK_COLOR; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/a")) {
                color = BR_TEXT; p += skip_tag(p); continue;
            }

            /* Title */
            if (is_tag(p, "title")) {
                in_title_tag = 1; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/title")) {
                in_title_tag = 0; p += skip_tag(p); continue;
            }

            /* Style/Script — icerigini atla */
            if (is_tag(p, "style") || is_tag(p, "script") ||
                is_tag(p, "head")) {
                skip_content = 1; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/style") || is_tag(p, "/script") ||
                is_tag(p, "/head")) {
                skip_content = 0; p += skip_tag(p); continue;
            }

            /* Pre */
            if (is_tag(p, "pre") || is_tag(p, "code")) {
                in_pre = 1; color = 0xAACC88;
                x = margin; y += 8;
                p += skip_tag(p); continue;
            }
            if (is_tag(p, "/pre") || is_tag(p, "/code")) {
                in_pre = 0; color = BR_TEXT;
                y += 8; x = margin;
                p += skip_tag(p); continue;
            }

            /* Liste */
            if (is_tag(p, "ul") || is_tag(p, "ol")) {
                y += 4; p += skip_tag(p); continue;
            }
            if (is_tag(p, "/ul") || is_tag(p, "/ol")) {
                y += 4; p += skip_tag(p); continue;
            }
            if (is_tag(p, "li")) {
                x = margin; y += 16;
                if (y >= cy && y < cy + ch) {
                    /* Bullet */
                    vesa_fill_rect(margin + 4, y + 5, 5, 5, BR_LINK_COLOR);
                }
                x = margin + 16;
                p += skip_tag(p); continue;
            }
            if (is_tag(p, "/li")) {
                p += skip_tag(p); continue;
            }

            /* HR */
            if (is_tag(p, "hr")) {
                y += 10;
                if (y >= cy && y < cy + ch) {
                    for (int hx = margin; hx < max_x; hx++)
                        vesa_putpixel(hx, y, 0x444466);
                }
                y += 10; x = margin;
                p += skip_tag(p); continue;
            }

            /* Bilinmeyen tag — atla */
            p += skip_tag(p);
            continue;
        }

        /* Skip mode */
        if (skip_content) { p++; continue; }

        /* Title icerigi */
        if (in_title_tag) {
            int tl = k_strlen(page_title);
            if (tl < 60 && *p >= 32 && *p < 127) {
                page_title[tl] = *p;
                page_title[tl + 1] = '\0';
            }
            p++;
            continue;
        }

        /* HTML entities */
        if (*p == '&') {
            char decoded = 0;
            int skip = 0;
            if (k_strncmp(p, "&amp;", 5) == 0) { decoded = '&'; skip = 5; }
            else if (k_strncmp(p, "&lt;", 4) == 0) { decoded = '<'; skip = 4; }
            else if (k_strncmp(p, "&gt;", 4) == 0) { decoded = '>'; skip = 4; }
            else if (k_strncmp(p, "&nbsp;", 6) == 0) { decoded = ' '; skip = 6; }
            else if (k_strncmp(p, "&quot;", 6) == 0) { decoded = '"'; skip = 6; }
            else { p++; continue; }

            if (x >= max_x) { x = margin; y += 16; }
            if (y >= cy && y < cy + ch)
                vesa_draw_char(x, y, decoded, color, BR_CONTENT_BG);
            x += 8;
            p += skip;
            continue;
        }

        /* Newline */
        if (*p == '\n') {
            if (in_pre) { x = margin; y += 16; }
            else if (x > margin) x += 4; /* HTML'de newline = bosluk */
            p++;
            continue;
        }
        if (*p == '\r') { p++; continue; }
        if (*p == '\t') {
            x += 32;
            p++;
            continue;
        }

        /* Normal karakter */
        if (*p >= 32 && *p < 127) {
            if (x >= max_x) { x = margin; y += 16; }
            if (y >= cy && y < cy + ch) {
                vesa_draw_char(x, y, *p, color, BR_CONTENT_BG);
                if (bold)
                    vesa_draw_char(x + 1, y, *p, color, BR_CONTENT_BG);
                if (heading == 1) {
                    vesa_draw_char(x + 1, y, *p, color, BR_CONTENT_BG);
                    vesa_draw_char(x, y + 1, *p, color, BR_CONTENT_BG);
                }
            }
            x += 8;
        }

        if (y > last_y) last_y = y;
        p++;
    }

    max_scroll = (last_y - cy + 50);
    if (max_scroll < 0) max_scroll = 0;
}

/* ======== Sayfa yukle ======== */

static void load_page(const char* url) {
    k_memset(page_body, 0, PAGE_MAX);
    k_memset(page_title, 0, 64);
    k_memset(page_error, 0, 64);
    page_body_len = 0;
    page_status = 0;
    is_html = 0;
    scroll_y = 0;
    max_scroll = 0;
    loading = 1;

    /* URL parse */
    char host[128], path[128];
    k_memset(host, 0, 128);
    k_memset(path, 0, 128);

    const char* u = url;
    if (k_strncmp(u, "http://", 7) == 0) u += 7;
    if (k_strncmp(u, "https://", 8) == 0) {
        k_strcpy(page_error, "HTTPS desteklenmiyor");
        page_status = -1;
        loading = 0;
        return;
    }

    int hi = 0;
    while (*u && *u != '/' && hi < 127) host[hi++] = *u++;
    host[hi] = '\0';
    if (*u == '/') k_strcpy(path, u);
    else k_strcpy(path, "/");

    if (k_strlen(host) == 0) {
        k_strcpy(page_error, "Gecersiz URL");
        page_status = -1;
        loading = 0;
        return;
    }

    k_strcpy(page_title, host);
    hist_push(url);

    http_response_t resp;
    int ret = http_get(host, path, &resp);
    loading = 0;

    if (ret != 0) {
        k_strcpy(page_error, resp.error_msg);
        page_status = ret;
        return;
    }

    page_status = resp.status_code;

    if (resp.body_len > 0) {
        int copy = resp.body_len;
        if (copy > PAGE_MAX - 1) copy = PAGE_MAX - 1;
        for (int i = 0; i < copy; i++) page_body[i] = resp.body[i];
        page_body[copy] = '\0';
        page_body_len = copy;
    }

    if (k_strncmp(resp.content_type, "text/html", 9) == 0) is_html = 1;
    else if (k_strncmp(page_body, "<!doctype", 9) == 0 ||
             k_strncmp(page_body, "<!DOCTYPE", 9) == 0 ||
             k_strncmp(page_body, "<html", 5) == 0 ||
             k_strncmp(page_body, "<HTML", 5) == 0) is_html = 1;
}

/* ======== Cizim ======== */

static void draw_browser(mouse_state_t* ms) {
    int sw = vesa_get_width(), sh = vesa_get_height();

    vesa_fill_screen(BR_BG);

    /* ---- Tab Bar (30px) ---- */
    vesa_fill_rect(0, 0, sw, 30, BR_TAB_BG);

    /* Aktif sekme */
    int tab_w = k_strlen(page_title[0] ? page_title : "Yeni Sekme") * 8 + 32;
    if (tab_w > 250) tab_w = 250;
    vesa_fill_rect(4, 0, tab_w, 30, BR_TAB_ACTIVE);
    /* Tab alt cizgisi (mavi) */
    vesa_fill_rect(4, 28, tab_w, 2, 0x0078D4);

    char tab_t[32];
    k_strcpy(tab_t, page_title[0] ? page_title : "Yeni Sekme");
    if (k_strlen(tab_t) > 28) tab_t[28] = '\0';
    vesa_draw_string(14, 8, tab_t, BR_TEXT, BR_TAB_ACTIVE);

    /* Kapat butonu */
    int cx = sw - 32, cby = 2;
    int ch_btn = (ms->x >= cx && ms->x < cx + 28 && ms->y >= cby && ms->y < cby + 26);
    vesa_fill_rect(cx, cby, 28, 26, ch_btn ? 0xE81123 : BR_TAB_BG);
    for (int i = 0; i < 8; i++) {
        vesa_putpixel(cx + 7 + i, cby + 6 + i, 0xFFFFFF);
        vesa_putpixel(cx + 8 + i, cby + 6 + i, 0xFFFFFF);
        vesa_putpixel(cx + 20 - i, cby + 6 + i, 0xFFFFFF);
        vesa_putpixel(cx + 19 - i, cby + 6 + i, 0xFFFFFF);
    }

    /* ---- Navigation Bar (40px) ---- */
    int ny = 30;
    vesa_fill_rect(0, ny, sw, 40, BR_NAV_BG);
    vesa_fill_rect(0, ny + 39, sw, 1, 0x2A2A48);

    /* Geri butonu */
    int bk_x = 10, bk_hover = (ms->x>=bk_x&&ms->x<bk_x+32&&ms->y>=ny+4&&ms->y<ny+36);
    vesa_fill_rect(bk_x, ny+4, 32, 32, bk_hover ? BR_BTN_HOVER : BR_NAV_BG);
    uint32_t bk_c = hist_pos > 0 ? BR_TEXT : 0x555566;
    vesa_draw_string(bk_x + 10, ny + 12, "<", bk_c, bk_hover ? BR_BTN_HOVER : BR_NAV_BG);

    /* Ileri butonu */
    int fw_x = 46, fw_hover = (ms->x>=fw_x&&ms->x<fw_x+32&&ms->y>=ny+4&&ms->y<ny+36);
    vesa_fill_rect(fw_x, ny+4, 32, 32, fw_hover ? BR_BTN_HOVER : BR_NAV_BG);
    uint32_t fw_c = hist_pos < hist_count - 1 ? BR_TEXT : 0x555566;
    vesa_draw_string(fw_x + 10, ny + 12, ">", fw_c, fw_hover ? BR_BTN_HOVER : BR_NAV_BG);

    /* Yenile butonu */
    int rf_x = 82, rf_hover = (ms->x>=rf_x&&ms->x<rf_x+32&&ms->y>=ny+4&&ms->y<ny+36);
    vesa_fill_rect(rf_x, ny+4, 32, 32, rf_hover ? BR_BTN_HOVER : BR_NAV_BG);
    vesa_draw_string(rf_x + 8, ny + 12, "R", BR_TEXT, rf_hover ? BR_BTN_HOVER : BR_NAV_BG);

    /* Ana sayfa butonu */
    int hm_x = 118, hm_hover = (ms->x>=hm_x&&ms->x<hm_x+32&&ms->y>=ny+4&&ms->y<ny+36);
    vesa_fill_rect(hm_x, ny+4, 32, 32, hm_hover ? BR_BTN_HOVER : BR_NAV_BG);
    /* Ev ikonu */
    vesa_fill_rect(hm_x + 8, ny + 18, 16, 10, 0x0078D4);
    vesa_fill_rect(hm_x + 11, ny + 12, 10, 6, 0x0078D4);

    /* Adres cubugu */
    int ax = 160, ay2 = ny + 6, aw = sw - 180, ah2 = 28;
    uint32_t addr_bg2 = addr_editing ? BR_ADDR_FOCUS : BR_ADDR_BG;
    uint32_t addr_bd = addr_editing ? BR_ADDR_BORDER : 0x333355;

    /* Yuvarlatilmis adres cubugu */
    vesa_fill_rect(ax + 2, ay2, aw - 4, ah2, addr_bg2);
    vesa_fill_rect(ax, ay2 + 2, 2, ah2 - 4, addr_bg2);
    vesa_fill_rect(ax + aw - 2, ay2 + 2, 2, ah2 - 4, addr_bg2);
    vesa_draw_rect_outline(ax, ay2, aw, ah2, addr_bd);

    /* Guvenlk simgesi */
    if (page_status == 200) {
        vesa_fill_rect(ax + 8, ay2 + 8, 8, 10, BR_SUCCESS);
        vesa_fill_rect(ax + 10, ay2 + 5, 4, 4, BR_SUCCESS);
    }

    /* Adres metni */
    int addr_text_x = ax + 22;
    vesa_draw_string(addr_text_x, ay2 + 7, addr_buf, BR_TEXT, addr_bg2);

    /* Cursor */
    if (addr_editing)
        vesa_fill_rect(addr_text_x + addr_len * 8, ay2 + 5, 2, 18, 0x0078D4);

    /* ---- Bookmark Bar (26px) ---- */
    int bby = ny + 40;
    vesa_fill_rect(0, bby, sw, 26, BR_BOOKMARK_BG);
    vesa_fill_rect(0, bby + 25, sw, 1, 0x2A2A48);

    for (int i = 0; i < BM_COUNT; i++) {
        int bx = 12 + i * 135;
        int bw = 125;
        int bmh = (ms->x >= bx && ms->x < bx + bw && ms->y >= bby + 2 && ms->y < bby + 24);
        uint32_t bmbg = bmh ? BR_BTN_HOVER : BR_BOOKMARK_BG;
        vesa_fill_rect(bx, bby + 3, bw, 20, bmbg);

        /* Favicon placeholder */
        vesa_fill_rect(bx + 4, bby + 7, 10, 10, 0x0078D4);
        vesa_draw_string(bx + 18, bby + 6, bm_names[i], 0xAAAABB, bmbg);
    }

    /* ---- Content Area ---- */
    int content_y = bby + 26;
    int content_h = sh - content_y - 26;
    vesa_fill_rect(0, content_y, sw, content_h, BR_CONTENT_BG);

    if (loading) {
        /* Yukleniyor animasyonu */
        vesa_draw_string(sw / 2 - 52, content_y + content_h / 2 - 20,
                         "Yukleniyor...", BR_LOADING, BR_CONTENT_BG);
        /* Progress bar */
        vesa_fill_rect(sw / 2 - 100, content_y + content_h / 2 + 10,
                       200, 4, 0x333355);
        vesa_fill_rect(sw / 2 - 100, content_y + content_h / 2 + 10,
                       120, 4, 0x0078D4);
    } else if (page_status < 0) {
        /* Hata sayfasi */
        int ey = content_y + 40;
        /* Buyuk X ikonu */
        for (int d = -20; d <= 20; d++) {
            for (int t = -2; t <= 2; t++) {
                vesa_putpixel(sw / 2 + d + t, ey + 30 + d, BR_ERROR);
                vesa_putpixel(sw / 2 - d + t, ey + 30 + d, BR_ERROR);
            }
        }
        ey += 70;
        vesa_draw_string(sw / 2 - 80, ey, "Sayfa yuklenemedi", BR_ERROR, BR_CONTENT_BG);
        ey += 24;
        vesa_draw_string(sw / 2 - k_strlen(page_error) * 4, ey,
                         page_error, BR_TEXT, BR_CONTENT_BG);
        ey += 20;
        char ec[20];
        k_strcpy(ec, "Hata kodu: ");
        k_itoa(page_status, ec + 11, 10);
        vesa_draw_string(sw / 2 - k_strlen(ec) * 4, ey, ec, BR_TEXT_DIM, BR_CONTENT_BG);
    } else if (page_body_len > 0) {
        /* Sayfa render */
        render_page(content_y, content_h);

        /* HTTP durum kodu */
        char sc[16];
        k_strcpy(sc, "HTTP ");
        k_itoa(page_status, sc + 5, 10);
        uint32_t sc_color = (page_status >= 200 && page_status < 300) ? BR_SUCCESS : BR_ERROR;
        vesa_draw_string(sw - 80, content_y + 4, sc, sc_color, BR_CONTENT_BG);

        /* Scroll bar */
        if (max_scroll > content_h) {
            int sb_track_h = content_h;
            int sb_h = (content_h * content_h) / max_scroll;
            if (sb_h < 30) sb_h = 30;
            int sb_y = content_y + (scroll_y * (sb_track_h - sb_h)) / (max_scroll - content_h);
            if (sb_y < content_y) sb_y = content_y;

            /* Track */
            vesa_fill_rect(sw - 10, content_y, 10, sb_track_h, 0x222240);
            /* Thumb */
            int sb_hover = (ms->x >= sw - 10 && ms->x < sw);
            vesa_fill_rect(sw - 8, sb_y, 6, sb_h, sb_hover ? 0x7777AA : 0x555577);
        }
    } else {
        /* Bos sayfa — hos geldin */
        int wy = content_y + 60;
        /* Logo */
        vesa_fill_rect(sw / 2 - 24, wy, 48, 40, 0x0078D4);
        vesa_fill_rect(sw / 2 - 20, wy + 4, 18, 14, BR_CONTENT_BG);
        vesa_fill_rect(sw / 2 + 2, wy + 4, 18, 14, BR_CONTENT_BG);
        vesa_fill_rect(sw / 2 - 20, wy + 22, 18, 14, BR_CONTENT_BG);
        vesa_fill_rect(sw / 2 + 2, wy + 22, 18, 14, BR_CONTENT_BG);

        wy += 56;
        vesa_draw_string(sw / 2 - 52, wy, "MyOS Browser", BR_HEADING1, BR_CONTENT_BG);
        wy += 24;
        vesa_draw_string(sw / 2 - 120, wy,
                         "Adres cubuguna bir URL yazin ve Enter'a basin",
                         BR_TEXT_DIM, BR_CONTENT_BG);
        wy += 30;
        vesa_draw_string(sw / 2 - 60, wy, "HTTP Siteler:", BR_TEXT, BR_CONTENT_BG);
        wy += 20;
        const char* examples[] = {"example.com", "info.cern.ch", "httpforever.com"};
        for (int i = 0; i < 3; i++) {
            vesa_fill_rect(sw / 2 - 8, wy + 5, 5, 5, 0x0078D4);
            vesa_draw_string(sw / 2 + 4, wy, examples[i], BR_LINK_COLOR, BR_CONTENT_BG);
            wy += 20;
        }
        wy += 16;
        vesa_draw_string(sw / 2 - 140, wy,
                         "Not: HTTPS siteleri henuz desteklenmemektedir.",
                         0x666677, BR_CONTENT_BG);
    }

    /* ---- Status Bar (26px) ---- */
    int sty = sh - 26;
    vesa_fill_rect(0, sty, sw, 26, BR_STATUS_BG);
    vesa_fill_rect(0, sty, sw, 1, 0x2A2A48);

    if (page_body_len > 0) {
        char sz[32];
        k_itoa(page_body_len, sz, 10);
        k_strcpy(sz + k_strlen(sz), " byte yuklendi");
        vesa_draw_string(10, sty + 6, sz, BR_TEXT_DIM, BR_STATUS_BG);
    } else if (loading) {
        vesa_draw_string(10, sty + 6, "Baglaniyor...", BR_LOADING, BR_STATUS_BG);
    }

    /* Scroll bilgisi */
    if (max_scroll > 0) {
        int pct = (scroll_y * 100) / (max_scroll > 0 ? max_scroll : 1);
        char sp[10];
        k_itoa(pct, sp, 10);
        k_strcpy(sp + k_strlen(sp), "%");
        vesa_draw_string(sw - 200, sty + 6, sp, BR_TEXT_DIM, BR_STATUS_BG);
    }

    vesa_draw_string(sw - 150, sty + 6, "MyOS Browser v0.2", 0x444466, BR_STATUS_BG);
}

/* ======== Ana Dongu ======== */

void app_browser(void) {
    mouse_state_t ms;
    int sw = vesa_get_width(), sh = vesa_get_height();

    k_memset(addr_buf, 0, ADDR_MAX);
    k_memset(history, 0, sizeof(history));
    addr_len = 0;
    addr_editing = 0;
    scroll_y = 0;
    max_scroll = 0;
    loading = 0;
    page_body_len = 0;
    page_status = 0;
    page_title[0] = '\0';
    page_error[0] = '\0';
    hist_pos = -1;
    hist_count = 0;

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();
        net_poll();

        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;

        /* Adres cubugu editing */
        if (addr_editing) {
            if (key == '\n') {
                addr_buf[addr_len] = '\0';
                addr_editing = 0;
                load_page(addr_buf);
            } else if (key == '\b') {
                if (addr_len > 0) addr_buf[--addr_len] = '\0';
            } else if (key >= 32 && key < 127 && addr_len < ADDR_MAX - 1) {
                addr_buf[addr_len++] = (char)key;
                addr_buf[addr_len] = '\0';
            }
        } else {
            /* Kısayollar */
            if (key == '\b') { go_back(); if (hist_pos >= 0) load_page(history[hist_pos]); }
        }

        /* Scroll — klavye */
        if (key == KEY_DOWN) {
            scroll_y += 40;
            if (scroll_y > max_scroll) scroll_y = max_scroll;
        }
        if (key == KEY_UP) {
            scroll_y -= 40;
            if (scroll_y < 0) scroll_y = 0;
        }

        /* Cizim */
        draw_browser(&ms);
        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;

        int ny = 30;

        /* Kapat */
        if (ms.x >= sw - 32 && ms.x < sw - 4 && ms.y >= 2 && ms.y < 28)
            return;

        /* Geri */
        if (ms.x >= 10 && ms.x < 42 && ms.y >= ny + 4 && ms.y < ny + 36) {
            go_back();
            if (hist_pos >= 0) load_page(history[hist_pos]);
        }

        /* Ileri */
        if (ms.x >= 46 && ms.x < 78 && ms.y >= ny + 4 && ms.y < ny + 36) {
            go_forward();
            if (hist_pos >= 0 && hist_pos < hist_count) load_page(history[hist_pos]);
        }

        /* Yenile */
        if (ms.x >= 82 && ms.x < 114 && ms.y >= ny + 4 && ms.y < ny + 36) {
            if (addr_len > 0) load_page(addr_buf);
        }

        /* Ana sayfa */
        if (ms.x >= 118 && ms.x < 150 && ms.y >= ny + 4 && ms.y < ny + 36) {
            k_strcpy(addr_buf, "example.com");
            addr_len = k_strlen(addr_buf);
            load_page(addr_buf);
        }

        /* Adres cubugu */
        if (ms.x >= 160 && ms.x < sw - 20 && ms.y >= ny + 6 && ms.y < ny + 34) {
            addr_editing = 1;
        } else {
            addr_editing = 0;
        }

        /* Yer imleri */
        int bby = ny + 40;
        for (int i = 0; i < BM_COUNT; i++) {
            int bx = 12 + i * 135;
            if (ms.x >= bx && ms.x < bx + 125 && ms.y >= bby + 2 && ms.y < bby + 24) {
                k_strcpy(addr_buf, bm_urls[i]);
                addr_len = k_strlen(addr_buf);
                load_page(addr_buf);
                break;
            }
        }

        /* Scroll bar tiklama */
        int content_y2 = bby + 26;
        int content_h2 = sh - content_y2 - 26;
        if (ms.x >= sw - 10 && ms.y >= content_y2 && ms.y < content_y2 + content_h2) {
            int click_pos = ms.y - content_y2;
            scroll_y = (click_pos * max_scroll) / content_h2;
            if (scroll_y < 0) scroll_y = 0;
            if (scroll_y > max_scroll) scroll_y = max_scroll;
        }
    }
}
