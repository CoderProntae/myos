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

static char addr_buf[ADDR_MAX];
static int  addr_len = 0;
static int  addr_editing = 0;
static int  scroll_y = 0;
static int  loading = 0;

/* Sayfa icerigi */
static char page_title[64];
static char page_body[PAGE_MAX];
static int  page_body_len = 0;
static int  page_status = 0;
static char page_error[64];
static int  is_html = 0;

/* Yer imleri */
static const char* bm_names[] = {"example.com", "info.cern.ch", "neverssl.com", "httpforever.com"};
static const char* bm_urls[]  = {"example.com", "info.cern.ch", "neverssl.com", "httpforever.com"};
#define BM_COUNT 4

/* ======== Basit HTML renderer ======== */

static int is_tag(const char* p, const char* tag) {
    int tl = k_strlen(tag);
    if (*p != '<') return 0;
    p++;
    for (int i = 0; i < tl; i++) {
        char a = p[i];
        char b = tag[i];
        /* Buyuk/kucuk harf esitle */
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
    }
    char next = p[tl];
    return (next == '>' || next == ' ' || next == '/');
}

static int skip_tag(const char* p) {
    int i = 0;
    while (p[i] && p[i] != '>') i++;
    if (p[i] == '>') i++;
    return i;
}

static void render_page(int content_y, int content_h) {
    int sw = vesa_get_width();
    int x = 30, y = content_y + 8 - scroll_y;
    int max_x = sw - 40;

    if (!is_html) {
        /* Duz metin */
        const char* p = page_body;
        while (*p && y < content_y + content_h) {
            if (y + 16 >= content_y) {
                if (*p == '\n') {
                    x = 30;
                    y += 16;
                } else if (*p == '\r') {
                    /* atla */
                } else if (*p >= 32 && *p < 127) {
                    if (x < max_x) {
                        vesa_draw_char(x, y, *p, 0xCCCCCC, 0x1A1A2E);
                        x += 8;
                    } else {
                        x = 30;
                        y += 16;
                        vesa_draw_char(x, y, *p, 0xCCCCCC, 0x1A1A2E);
                        x += 8;
                    }
                }
            } else {
                if (*p == '\n') { x = 30; y += 16; }
            }
            p++;
        }
        return;
    }

    /* HTML render */
    const char* p = page_body;
    uint32_t color = 0xCCCCCC;
    int bold = 0;
    int in_title = 0;
    int heading = 0;
    int skip_content = 0;  /* style/script icerigi atla */

    while (*p && y < content_y + content_h + scroll_y) {
        if (*p == '<') {
            /* Tag kontrol */
            if (is_tag(p, "h1") || is_tag(p, "h2") || is_tag(p, "h3")) {
                heading = 1;
                color = 0x55CCFF;
                x = 30;
                y += 8;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "/h1") || is_tag(p, "/h2") || is_tag(p, "/h3")) {
                heading = 0;
                color = 0xCCCCCC;
                y += 24;
                x = 30;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "b") || is_tag(p, "strong")) {
                bold = 1; color = 0xFFFFFF;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "/b") || is_tag(p, "/strong")) {
                bold = 0; color = 0xCCCCCC;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "p") || is_tag(p, "div") || is_tag(p, "br")) {
                x = 30;
                y += 18;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "/p") || is_tag(p, "/div")) {
                y += 8;
                x = 30;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "a")) {
                color = 0x5599FF;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "/a")) {
                color = 0xCCCCCC;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "title")) {
                in_title = 1;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "/title")) {
                in_title = 0;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "style") || is_tag(p, "script")) {
                skip_content = 1;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "/style") || is_tag(p, "/script")) {
                skip_content = 0;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "li")) {
                x = 30;
                y += 16;
                /* Bullet */
                if (y >= content_y && y < content_y + content_h) {
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++)
                            if (dx*dx+dy*dy<=4)
                                vesa_putpixel(x+dx+4, y+dy+7, 0x0078D4);
                }
                x = 44;
                p += skip_tag(p);
                continue;
            }
            if (is_tag(p, "hr")) {
                y += 8;
                if (y >= content_y && y < content_y + content_h) {
                    for (int hx = 30; hx < max_x; hx++)
                        vesa_putpixel(hx, y, 0x444466);
                }
                y += 8;
                x = 30;
                p += skip_tag(p);
                continue;
            }

            /* Bilinmeyen tag — atla */
            p += skip_tag(p);
            continue;
        }

        /* Skip content (style/script) */
        if (skip_content) { p++; continue; }

        /* Title icerigi */
        if (in_title) {
            /* page_title'a yaz */
            int tl = k_strlen(page_title);
            if (tl < 63 && *p != '\n' && *p != '\r') {
                page_title[tl] = *p;
                page_title[tl+1] = '\0';
            }
            p++;
            continue;
        }

        /* &amp; &lt; &gt; &nbsp; */
        if (*p == '&') {
            if (k_strncmp(p, "&amp;", 5) == 0) {
                if (y >= content_y && y < content_y + content_h && x < max_x)
                    vesa_draw_char(x, y, '&', color, 0x1A1A2E);
                x += 8; p += 5; continue;
            }
            if (k_strncmp(p, "&lt;", 4) == 0) {
                if (y >= content_y && y < content_y + content_h && x < max_x)
                    vesa_draw_char(x, y, '<', color, 0x1A1A2E);
                x += 8; p += 4; continue;
            }
            if (k_strncmp(p, "&gt;", 4) == 0) {
                if (y >= content_y && y < content_y + content_h && x < max_x)
                    vesa_draw_char(x, y, '>', color, 0x1A1A2E);
                x += 8; p += 4; continue;
            }
            if (k_strncmp(p, "&nbsp;", 6) == 0) {
                x += 8; p += 6; continue;
            }
            /* Bilinmeyen entity */
            p++;
            continue;
        }

        /* Normal karakter */
        if (*p == '\n' || *p == '\r') {
            /* HTML'de newline = bosluk */
            if (x > 30) x += 8;
            p++;
            continue;
        }

        if (*p >= 32 && *p < 127) {
            if (x >= max_x) {
                x = 30;
                y += 16;
            }
            if (y >= content_y && y < content_y + content_h) {
                vesa_draw_char(x, y, *p, color, 0x1A1A2E);
                if (bold && x + 1 < max_x)
                    vesa_draw_char(x + 1, y, *p, color, 0x1A1A2E);
            }
            x += 8;
        }

        p++;
    }
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
    loading = 1;

    /* URL parse: host ve path ayir */
    char host[128];
    char path[128];
    k_memset(host, 0, 128);
    k_memset(path, 0, 128);

    const char* u = url;

    /* "http://" atla */
    if (k_strncmp(u, "http://", 7) == 0) u += 7;

    /* Host */
    int hi = 0;
    while (*u && *u != '/' && hi < 127) {
        host[hi++] = *u++;
    }
    host[hi] = '\0';

    /* Path */
    if (*u == '/') {
        k_strcpy(path, u);
    } else {
        k_strcpy(path, "/");
    }

    /* Bos host */
    if (k_strlen(host) == 0) {
        k_strcpy(page_error, "Gecersiz URL");
        page_status = -1;
        loading = 0;
        return;
    }

    k_strcpy(page_title, host);

    /* HTTP GET */
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
        for (int i = 0; i < copy; i++)
            page_body[i] = resp.body[i];
        page_body[copy] = '\0';
        page_body_len = copy;
    }

    /* HTML mi? */
    if (k_strncmp(resp.content_type, "text/html", 9) == 0) {
        is_html = 1;
    } else if (page_body_len > 0) {
        /* Icerik kontrolu */
        if (k_strncmp(page_body, "<!doctype", 9) == 0 ||
            k_strncmp(page_body, "<!DOCTYPE", 9) == 0 ||
            k_strncmp(page_body, "<html", 5) == 0 ||
            k_strncmp(page_body, "<HTML", 5) == 0) {
            is_html = 1;
        }
    }
}

/* ======== Cizim ======== */

static void draw_browser(mouse_state_t* ms) {
    int sw = vesa_get_width(), sh = vesa_get_height();

    vesa_fill_screen(0x1A1A2E);

    /* Baslik */
    vesa_fill_rect(0, 0, sw, 30, 0x1F1F33);
    vesa_fill_rect(0, 29, sw, 1, 0x333355);

    /* Sekme */
    vesa_fill_rect(4, 2, 220, 26, 0x2B2B3D);
    vesa_draw_rect_outline(4, 2, 220, 26, 0x444466);
    char tab_text[40];
    if (page_title[0])
        k_strcpy(tab_text, page_title);
    else
        k_strcpy(tab_text, "Yeni Sekme");
    /* Uzun baslik kes */
    if (k_strlen(tab_text) > 24) tab_text[24] = '\0';
    vesa_draw_string(12, 8, tab_text, 0xCCCCCC, 0x2B2B3D);

    /* Kapat butonu */
    vesa_fill_rect(sw - 30, 2, 26, 26, COLOR_CLOSE_BTN);
    for (int i = 0; i < 8; i++) {
        vesa_putpixel(sw-24+i, 8+i, 0xFFFFFF);
        vesa_putpixel(sw-24+i, 9+i, 0xFFFFFF);
        vesa_putpixel(sw-17-i, 8+i, 0xFFFFFF);
        vesa_putpixel(sw-17-i, 9+i, 0xFFFFFF);
    }

    /* Nav bar */
    int ny = 30;
    vesa_fill_rect(0, ny, sw, 36, 0x252540);
    vesa_fill_rect(0, ny+35, sw, 1, 0x333355);

    /* Geri */
    int bh1 = (ms->x>=8&&ms->x<36&&ms->y>=ny+4&&ms->y<ny+30);
    vesa_fill_rect(8,ny+4,28,28,bh1?0x3A3A5A:0x252540);
    vesa_draw_string(18,ny+10,"<",0xCCCCCC,bh1?0x3A3A5A:0x252540);

    /* Yenile */
    int bh2 = (ms->x>=44&&ms->x<72&&ms->y>=ny+4&&ms->y<ny+30);
    vesa_fill_rect(44,ny+4,28,28,bh2?0x3A3A5A:0x252540);
    vesa_draw_string(52,ny+10,"O",0xCCCCCC,bh2?0x3A3A5A:0x252540);

    /* Ana sayfa */
    int bh3 = (ms->x>=80&&ms->x<108&&ms->y>=ny+4&&ms->y<ny+30);
    vesa_fill_rect(80,ny+4,28,28,bh3?0x3A3A5A:0x252540);
    vesa_draw_string(88,ny+10,"H",0xCCCCCC,bh3?0x3A3A5A:0x252540);

    /* Adres cubugu */
    int ax=116, ay=ny+5, aw=sw-130, ah=26;
    uint32_t abg = addr_editing ? 0x1A1A2A : 0x1E1E30;
    vesa_fill_rect(ax,ay,aw,ah,abg);
    vesa_draw_rect_outline(ax,ay,aw,ah,addr_editing?0x0078D4:0x444466);
    vesa_draw_string(ax+8, ay+5, addr_buf, 0xCCCCCC, abg);
    if (addr_editing)
        vesa_fill_rect(ax+8+addr_len*8, ay+4, 2, 18, 0x0078D4);

    /* Yer imleri */
    int by = ny + 36;
    vesa_fill_rect(0, by, sw, 24, 0x1E1E30);
    vesa_fill_rect(0, by+23, sw, 1, 0x333355);
    for (int i = 0; i < BM_COUNT; i++) {
        int bx = 8 + i * 130;
        int bmh = (ms->x>=bx&&ms->x<bx+120&&ms->y>=by+2&&ms->y<by+20);
        vesa_fill_rect(bx,by+2,120,20,bmh?0x3A3A5A:0x1E1E30);
        vesa_draw_string(bx+4,by+5,bm_names[i],0xAAAACC,bmh?0x3A3A5A:0x1E1E30);
    }

    /* Icerik alani */
    int cy = by + 24;
    int ch = sh - cy - 24;
    vesa_fill_rect(0, cy, sw, ch, 0x1A1A2E);

    if (loading) {
        vesa_draw_string(sw/2-60, cy+ch/2-8, "Yukleniyor...", 0xFFFF55, 0x1A1A2E);
    } else if (page_status < 0) {
        /* Hata */
        vesa_draw_string(30, cy+20, "Hata!", 0xFF5555, 0x1A1A2E);
        vesa_draw_string(30, cy+40, page_error, 0xCCCCCC, 0x1A1A2E);
        char sc[10];
        k_strcpy(sc, "Kod: ");
        k_itoa(page_status, sc+5, 10);
        vesa_draw_string(30, cy+60, sc, 0x888888, 0x1A1A2E);
    } else if (page_body_len > 0) {
        /* Sayfa render */
        render_page(cy, ch);

        /* Status */
        char sc2[32];
        k_strcpy(sc2, "HTTP ");
        k_itoa(page_status, sc2+5, 10);
        vesa_draw_string(sw-120, cy+4, sc2,
            page_status==200?0x55FF55:0xFF5555, 0x1A1A2E);
    } else {
        vesa_draw_string(30, cy+20, "MyOS Browser", 0x55CCFF, 0x1A1A2E);
        vesa_draw_string(30, cy+44, "Adres cubuguna bir URL yazin ve Enter'a basin.", 0xAAAAAA, 0x1A1A2E);
        vesa_draw_string(30, cy+68, "Ornekler:", 0xCCCCCC, 0x1A1A2E);
        vesa_draw_string(30, cy+88, "  example.com", 0x5599FF, 0x1A1A2E);
        vesa_draw_string(30, cy+108,"  info.cern.ch", 0x5599FF, 0x1A1A2E);
        vesa_draw_string(30, cy+128,"  neverssl.com", 0x5599FF, 0x1A1A2E);
        vesa_draw_string(30, cy+128+20,"  httpforever.com", 0x5599FF, 0x1A1A2E);
        vesa_draw_string(30, cy+180,"Not: Sadece HTTP siteleri desteklenir.", 0x888888, 0x1A1A2E);
    }

    /* Durum cubugu */
    int sty = sh - 24;
    vesa_fill_rect(0, sty, sw, 24, 0x1F1F33);
    vesa_fill_rect(0, sty, sw, 1, 0x333355);
    if (page_body_len > 0) {
        char sz[32];
        k_itoa(page_body_len, sz, 10);
        k_strcpy(sz+k_strlen(sz), " byte");
        vesa_draw_string(8, sty+4, sz, 0x888888, 0x1F1F33);
    }
    vesa_draw_string(sw-180, sty+4, "MyOS Browser v0.2", 0x555577, 0x1F1F33);
}

/* ======== Ana Dongu ======== */

void app_browser(void) {
    mouse_state_t ms;
    int sw = vesa_get_width(), sh = vesa_get_height();

    k_memset(addr_buf, 0, ADDR_MAX);
    addr_len = 0;
    addr_editing = 0;
    scroll_y = 0;
    loading = 0;
    page_body_len = 0;
    page_status = 0;
    page_title[0] = '\0';
    page_error[0] = '\0';

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();
        net_poll();

        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;

        /* Adres cubugu */
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
        }

        /* Scroll */
        if (key == KEY_DOWN) scroll_y += 30;
        if (key == KEY_UP) { scroll_y -= 30; if (scroll_y < 0) scroll_y = 0; }

        /* Cizim */
        draw_browser(&ms);
        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;

        int ny = 30;

        /* Kapat */
        if (ms.x >= sw-30 && ms.x < sw-4 && ms.y >= 2 && ms.y < 28)
            return;

        /* Yenile */
        if (ms.x>=44 && ms.x<72 && ms.y>=ny+4 && ms.y<ny+30) {
            if (addr_len > 0) load_page(addr_buf);
        }

        /* Ana sayfa */
        if (ms.x>=80 && ms.x<108 && ms.y>=ny+4 && ms.y<ny+30) {
            k_strcpy(addr_buf, "example.com");
            addr_len = k_strlen(addr_buf);
            load_page(addr_buf);
        }

        /* Adres cubugu tikla */
        if (ms.x >= 116 && ms.x < sw-14 && ms.y >= ny+5 && ms.y < ny+31) {
            addr_editing = 1;
        } else {
            addr_editing = 0;
        }

        /* Yer imleri */
        int bmy = ny + 36;
        for (int i = 0; i < BM_COUNT; i++) {
            int bx = 8 + i * 130;
            if (ms.x>=bx && ms.x<bx+120 && ms.y>=bmy+2 && ms.y<bmy+20) {
                k_strcpy(addr_buf, bm_urls[i]);
                addr_len = k_strlen(addr_buf);
                load_page(addr_buf);
                break;
            }
        }
    }
}
