#include "browser.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"

/* ======== SAYFA SISTEMI ======== */

#define MAX_PAGES    8
#define MAX_CONTENT  2048
#define MAX_URL      128
#define ADDR_MAX     128

/* Basit HTML render komutu */
typedef struct {
    int type;        /* 0=text 1=heading 2=bold 3=link 4=hr 5=list 6=blank */
    int level;       /* heading seviyesi (1-3) */
    char text[128];
    uint32_t color;
} render_cmd_t;

#define MAX_CMDS 64

typedef struct {
    char url[MAX_URL];
    char title[64];
    render_cmd_t cmds[MAX_CMDS];
    int cmd_count;
} page_t;

static page_t pages[MAX_PAGES];
static int page_count = 0;
static int current_page = 0;
static int history[16];
static int hist_pos = 0;
static int hist_count = 0;
static char addr_buf[ADDR_MAX];
static int addr_len = 0;
static int addr_editing = 0;
static int scroll_y = 0;

/* ======== SAYFA EKLEME ======== */

static void add_cmd(page_t* p, int type, int level, const char* text, uint32_t color) {
    if (p->cmd_count >= MAX_CMDS) return;
    render_cmd_t* c = &p->cmds[p->cmd_count++];
    c->type = type;
    c->level = level;
    c->color = color;
    if (text) {
        int i = 0;
        while (text[i] && i < 127) { c->text[i] = text[i]; i++; }
        c->text[i] = '\0';
    } else {
        c->text[0] = '\0';
    }
}

static void init_pages(void) {
    page_t* p;

    /* Sayfa 0: Ana Sayfa */
    p = &pages[0];
    k_strcpy(p->url, "myos://home");
    k_strcpy(p->title, "Ana Sayfa");
    p->cmd_count = 0;
    add_cmd(p, 1, 1, "MyOS Browser'a Hosgeldiniz!", 0x55CCFF);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 0, 0, "Bu, MyOS isletim sisteminin yerlesik web tarayicisidir.", 0xCCCCCC);
    add_cmd(p, 0, 0, "Su an icin ag destegi bulunmamaktadir.", 0xAAAAAA);
    add_cmd(p, 0, 0, "Asagidaki sayfalari ziyaret edebilirsiniz:", 0xCCCCCC);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Sayfalar", 0x55FF55);
    add_cmd(p, 3, 0, "myos://about    - Hakkinda", 0x5599FF);
    add_cmd(p, 3, 0, "myos://help     - Yardim", 0x5599FF);
    add_cmd(p, 3, 0, "myos://sysinfo  - Sistem Bilgisi", 0x5599FF);
    add_cmd(p, 3, 0, "myos://news     - Haberler", 0x5599FF);
    add_cmd(p, 3, 0, "myos://demo     - HTML Demo", 0x5599FF);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Hizli Erisim", 0xFFFF55);
    add_cmd(p, 0, 0, "Adres cubuguna sayfa adresini yazin ve Enter'a basin.", 0xAAAAAA);
    add_cmd(p, 0, 0, "Geri/Ileri butonlari ile gecmiste gezinebilirsiniz.", 0xAAAAAA);

    /* Sayfa 1: Hakkinda */
    p = &pages[1];
    k_strcpy(p->url, "myos://about");
    k_strcpy(p->title, "Hakkinda");
    p->cmd_count = 0;
    add_cmd(p, 1, 1, "MyOS Browser", 0x55CCFF);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 2, 0, "Versiyon: 0.1.0", 0xFFFFFF);
    add_cmd(p, 0, 0, "Motor: MyOS Render Engine", 0xCCCCCC);
    add_cmd(p, 0, 0, "Dil: C (sifirdan yazildi)", 0xCCCCCC);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Ozellikler", 0x55FF55);
    add_cmd(p, 5, 0, "Adres cubugu ve sayfa gezintisi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Geri/Ileri gecmis destegi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Yerlesik sayfa sistemi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Klavye ve mouse destegi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Kaydirma (scroll) destegi", 0xCCCCCC);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Gelecek Planlar", 0xFFFF55);
    add_cmd(p, 5, 0, "TCP/IP ag destegi", 0xAAAAAA);
    add_cmd(p, 5, 0, "HTTP/HTTPS istemci", 0xAAAAAA);
    add_cmd(p, 5, 0, "HTML/CSS parser", 0xAAAAAA);
    add_cmd(p, 5, 0, "JavaScript engine", 0xAAAAAA);

    /* Sayfa 2: Yardim */
    p = &pages[2];
    k_strcpy(p->url, "myos://help");
    k_strcpy(p->title, "Yardim");
    p->cmd_count = 0;
    add_cmd(p, 1, 1, "Yardim", 0x55CCFF);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Klavye Kisayollari", 0x55FF55);
    add_cmd(p, 5, 0, "ESC        - Tarayiciyi kapat", 0xCCCCCC);
    add_cmd(p, 5, 0, "Alt+F4     - Tarayiciyi kapat", 0xCCCCCC);
    add_cmd(p, 5, 0, "Backspace  - Onceki sayfa", 0xCCCCCC);
    add_cmd(p, 5, 0, "Enter      - Adresi yukle", 0xCCCCCC);
    add_cmd(p, 5, 0, "Scroll     - Sayfa kaydir (buton)", 0xCCCCCC);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Adresler", 0xFFFF55);
    add_cmd(p, 5, 0, "myos://home    - Ana sayfa", 0x5599FF);
    add_cmd(p, 5, 0, "myos://about   - Hakkinda", 0x5599FF);
    add_cmd(p, 5, 0, "myos://help    - Bu sayfa", 0x5599FF);
    add_cmd(p, 5, 0, "myos://sysinfo - Sistem bilgisi", 0x5599FF);
    add_cmd(p, 5, 0, "myos://news    - Haberler", 0x5599FF);
    add_cmd(p, 5, 0, "myos://demo    - HTML Demo", 0x5599FF);

    /* Sayfa 3: Sistem Bilgisi */
    p = &pages[3];
    k_strcpy(p->url, "myos://sysinfo");
    k_strcpy(p->title, "Sistem Bilgisi");
    p->cmd_count = 0;
    add_cmd(p, 1, 1, "Sistem Bilgisi", 0x55CCFF);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);

    /* CPU bilgisi - calisma zamaninda doldurulacak */
    char vendor[13];
    uint32_t ebx, ecx, edx;
    __asm__ __volatile__("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    *((uint32_t*)&vendor[0]) = ebx;
    *((uint32_t*)&vendor[4]) = edx;
    *((uint32_t*)&vendor[8]) = ecx;
    vendor[12] = '\0';

    char cpu_line[64];
    k_strcpy(cpu_line, "CPU: ");
    k_strcpy(cpu_line + 5, vendor);
    add_cmd(p, 2, 0, cpu_line, 0x55FF55);

    monitor_info_t* mi = vesa_get_monitor_info();
    char adapt_line[64];
    k_strcpy(adapt_line, "Adaptor: ");
    k_strcpy(adapt_line + 9, mi->adapter_name);
    add_cmd(p, 0, 0, adapt_line, 0xCCCCCC);

    char mon_line[64];
    k_strcpy(mon_line, "Monitor: ");
    k_strcpy(mon_line + 9, mi->monitor_name);
    add_cmd(p, 0, 0, mon_line, 0xCCCCCC);

    add_cmd(p, 0, 0, mi->is_virtual ? "Ortam: Sanal Makine" : "Ortam: Fiziksel", 0xFFFF55);

    char vram_line[32];
    k_strcpy(vram_line, "VRAM: ");
    k_itoa((int)(mi->vram_bytes / 1024 / 1024), vram_line + 6, 10);
    k_strcpy(vram_line + k_strlen(vram_line), " MB");
    add_cmd(p, 0, 0, vram_line, 0xCCCCCC);

    char res_line[32];
    k_strcpy(res_line, "Cozunurluk: ");
    k_itoa(vesa_get_width(), res_line + 12, 10);
    res_line[k_strlen(res_line)] = 'x';
    k_itoa(vesa_get_height(), res_line + k_strlen(res_line), 10);
    add_cmd(p, 0, 0, res_line, 0xCCCCCC);

    char bpp_line[32];
    k_strcpy(bpp_line, "Renk: ");
    k_itoa(vesa_get_depth(), bpp_line + 6, 10);
    k_strcpy(bpp_line + k_strlen(bpp_line), "-bit");
    add_cmd(p, 0, 0, bpp_line, 0x55FFFF);

    /* Sayfa 4: Haberler */
    p = &pages[4];
    k_strcpy(p->url, "myos://news");
    k_strcpy(p->title, "Haberler");
    p->cmd_count = 0;
    add_cmd(p, 1, 1, "MyOS Haberler", 0x55CCFF);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "v0.3 - Buyuk Guncelleme!", 0x55FF55);
    add_cmd(p, 0, 0, "Tarih: 2025", 0x888888);
    add_cmd(p, 5, 0, "VESA grafik modu eklendi (800x600x32)", 0xCCCCCC);
    add_cmd(p, 5, 0, "Mouse destegi iyilestirildi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Dosya Gezgini eklendi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Not Defteri eklendi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Hesap Makinesi eklendi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Web Tarayicisi eklendi", 0xCCCCCC);
    add_cmd(p, 5, 0, "Gercek monitor algilama aktif", 0xCCCCCC);
    add_cmd(p, 5, 0, "Renk derinligi ve Hz ayarlari", 0xCCCCCC);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Gelecek Planlar", 0xFFFF55);
    add_cmd(p, 5, 0, "Ag destegi (TCP/IP)", 0xAAAAAA);
    add_cmd(p, 5, 0, "Gercek web sayfasi yuklemesi", 0xAAAAAA);
    add_cmd(p, 5, 0, "Dosya sistemi (FAT32)", 0xAAAAAA);

    /* Sayfa 5: HTML Demo */
    p = &pages[5];
    k_strcpy(p->url, "myos://demo");
    k_strcpy(p->title, "HTML Demo");
    p->cmd_count = 0;
    add_cmd(p, 1, 1, "HTML Render Demo", 0x55CCFF);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Baslik Seviye 2", 0x55FF55);
    add_cmd(p, 1, 3, "Baslik Seviye 3", 0x44CC44);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 0, 0, "Bu normal bir paragraf metnidir.", 0xCCCCCC);
    add_cmd(p, 2, 0, "Bu kalin (bold) bir metindir.", 0xFFFFFF);
    add_cmd(p, 3, 0, "Bu bir link metnidir (myos://home)", 0x5599FF);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 4, 0, 0, 0x444466);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 1, 2, "Liste Ornegi", 0xFFFF55);
    add_cmd(p, 5, 0, "Birinci madde", 0xCCCCCC);
    add_cmd(p, 5, 0, "Ikinci madde", 0xCCCCCC);
    add_cmd(p, 5, 0, "Ucuncu madde", 0xCCCCCC);
    add_cmd(p, 6, 0, 0, 0);
    add_cmd(p, 0, 0, "Renk ornekleri:", 0xCCCCCC);
    add_cmd(p, 0, 0, "Kirmizi metin", 0xFF5555);
    add_cmd(p, 0, 0, "Yesil metin", 0x55FF55);
    add_cmd(p, 0, 0, "Mavi metin", 0x5555FF);
    add_cmd(p, 0, 0, "Sari metin", 0xFFFF55);

    page_count = 6;
}

/* ======== SAYFA BULMA ======== */
static int find_page(const char* url) {
    for (int i = 0; i < page_count; i++) {
        if (k_strcmp(pages[i].url, url) == 0)
            return i;
    }
    return -1;
}

static void navigate(const char* url) {
    int idx = find_page(url);
    if (idx >= 0) {
        current_page = idx;
        scroll_y = 0;
        /* Gecmise ekle */
        hist_pos++;
        if (hist_pos >= 16) hist_pos = 15;
        history[hist_pos] = idx;
        hist_count = hist_pos + 1;
        /* Adres cubugunu guncelle */
        k_strcpy(addr_buf, url);
        addr_len = k_strlen(addr_buf);
    }
}

static void go_back(void) {
    if (hist_pos > 0) {
        hist_pos--;
        current_page = history[hist_pos];
        scroll_y = 0;
        k_strcpy(addr_buf, pages[current_page].url);
        addr_len = k_strlen(addr_buf);
    }
}

static void go_forward(void) {
    if (hist_pos < hist_count - 1) {
        hist_pos++;
        current_page = history[hist_pos];
        scroll_y = 0;
        k_strcpy(addr_buf, pages[current_page].url);
        addr_len = k_strlen(addr_buf);
    }
}

/* ======== CIZIM ======== */

static void draw_browser(mouse_state_t* ms) {
    int sw = vesa_get_width(), sh = vesa_get_height();
    int wx = 0, wy = 0, ww = sw, wh = sh;

    /* Arka plan */
    vesa_fill_screen(0x1A1A2E);

    /* Baslik cubugu */
    vesa_fill_rect(wx, wy, ww, 30, 0x1F1F33);
    vesa_fill_rect(wx, wy + 29, ww, 1, 0x333355);

    /* Sekme */
    vesa_fill_rect(wx + 4, wy + 2, 200, 26, 0x2B2B3D);
    vesa_draw_rect_outline(wx + 4, wy + 2, 200, 26, 0x444466);
    vesa_draw_string(wx + 12, wy + 8, pages[current_page].title, 0xCCCCCC, 0x2B2B3D);
    /* Sekme kapat */
    vesa_draw_string(wx + 186, wy + 8, "x", 0x888888, 0x2B2B3D);

    /* + butonu */
    vesa_draw_string(wx + 212, wy + 8, "+", 0x888888, 0x1F1F33);

    /* Pencere kontrolleri */
    vesa_fill_rect(wx + ww - 30, wy + 2, 26, 26, COLOR_CLOSE_BTN);
    draw_close_button(wx + ww - 26, wy + 6, COLOR_CLOSE_BTN);

    /* Navigasyon cubugu */
    int ny = wy + 30;
    vesa_fill_rect(wx, ny, ww, 36, 0x252540);
    vesa_fill_rect(wx, ny + 35, ww, 1, 0x333355);

    /* Geri butonu */
    int back_hover = (ms->x >= wx + 8 && ms->x < wx + 32 && ms->y >= ny + 4 && ms->y < ny + 30);
    uint32_t back_bg = back_hover ? 0x3A3A5A : 0x252540;
    vesa_fill_rect(wx + 8, ny + 4, 28, 28, back_bg);
    vesa_draw_string(wx + 16, ny + 10, "<", hist_pos > 0 ? 0xCCCCCC : 0x555555, back_bg);

    /* Ileri butonu */
    int fwd_hover = (ms->x >= wx + 40 && ms->x < wx + 64 && ms->y >= ny + 4 && ms->y < ny + 30);
    uint32_t fwd_bg = fwd_hover ? 0x3A3A5A : 0x252540;
    vesa_fill_rect(wx + 40, ny + 4, 28, 28, fwd_bg);
    vesa_draw_string(wx + 48, ny + 10, ">", hist_pos < hist_count - 1 ? 0xCCCCCC : 0x555555, fwd_bg);

    /* Yenile butonu */
    int ref_hover = (ms->x >= wx + 72 && ms->x < wx + 96 && ms->y >= ny + 4 && ms->y < ny + 30);
    uint32_t ref_bg = ref_hover ? 0x3A3A5A : 0x252540;
    vesa_fill_rect(wx + 72, ny + 4, 28, 28, ref_bg);
    vesa_draw_string(wx + 78, ny + 10, "O", 0xCCCCCC, ref_bg);

    /* Ana sayfa butonu */
    int home_hover = (ms->x >= wx + 104 && ms->x < wx + 128 && ms->y >= ny + 4 && ms->y < ny + 30);
    uint32_t home_bg = home_hover ? 0x3A3A5A : 0x252540;
    vesa_fill_rect(wx + 104, ny + 4, 28, 28, home_bg);
    vesa_draw_string(wx + 108, ny + 10, "H", 0xCCCCCC, home_bg);

    /* Adres cubugu */
    int ax = wx + 140, ay = ny + 5, aw = ww - 160, ah = 26;
    uint32_t addr_bg = addr_editing ? 0x1A1A2A : 0x1E1E30;
    vesa_fill_rect(ax, ay, aw, ah, addr_bg);
    vesa_draw_rect_outline(ax, ay, aw, ah, addr_editing ? 0x0078D4 : 0x444466);

    /* Guvenlk ikonu */
    vesa_draw_string(ax + 6, ay + 5, "O", 0x55FF55, addr_bg);

    /* Adres metni */
    vesa_draw_string(ax + 22, ay + 5, addr_buf, 0xCCCCCC, addr_bg);

    /* Cursor */
    if (addr_editing) {
        vesa_fill_rect(ax + 22 + addr_len * 8, ay + 4, 2, 18, 0x0078D4);
    }

    /* Yer imleri cubugu */
    int by2 = ny + 36;
    vesa_fill_rect(wx, by2, ww, 24, 0x1E1E30);
    vesa_fill_rect(wx, by2 + 23, ww, 1, 0x333355);

    const char* bmarks[] = {"Ana Sayfa", "Hakkinda", "Yardim", "Haberler", "Demo"};
    const char* burls[] = {"myos://home", "myos://about", "myos://help", "myos://news", "myos://demo"};
    for (int i = 0; i < 5; i++) {
        int bx = wx + 8 + i * 100;
        int bm_hover = (ms->x >= bx && ms->x < bx + 90 && ms->y >= by2 + 2 && ms->y < by2 + 20);
        uint32_t bm_bg = bm_hover ? 0x3A3A5A : 0x1E1E30;
        vesa_fill_rect(bx, by2 + 2, 90, 20, bm_bg);
        vesa_draw_string(bx + 4, by2 + 5, bmarks[i], 0xAAAACC, bm_bg);
    }

    /* Icerik alani */
    int cy = by2 + 24;
    int content_h = sh - cy - 24;
    vesa_fill_rect(wx, cy, ww, content_h, 0x1A1A2E);

    /* Sayfa render */
    page_t* pg = &pages[current_page];
    int ry = cy + 12 - scroll_y;

    for (int i = 0; i < pg->cmd_count; i++) {
        render_cmd_t* cmd = &pg->cmds[i];

        if (ry > cy + content_h) break;
        if (ry + 20 < cy) {
            /* Satir yuksekligini hesapla ama cizme */
            if (cmd->type == 1) ry += (cmd->level == 1 ? 28 : (cmd->level == 2 ? 24 : 20));
            else if (cmd->type == 4) ry += 12;
            else if (cmd->type == 6) ry += 8;
            else ry += 18;
            continue;
        }

        int rx = wx + 30;

        switch (cmd->type) {
            case 0: /* Normal metin */
                vesa_draw_string(rx, ry, cmd->text, cmd->color, 0x1A1A2E);
                ry += 18;
                break;
            case 1: /* Heading */
                if (cmd->level == 1) {
                    /* H1 - buyuk */
                    for (int dx = 0; dx < 2; dx++)
                        vesa_draw_string(rx + dx, ry, cmd->text, cmd->color, 0x1A1A2E);
                    ry += 28;
                } else if (cmd->level == 2) {
                    vesa_draw_string(rx, ry, cmd->text, cmd->color, 0x1A1A2E);
                    ry += 24;
                } else {
                    vesa_draw_string(rx, ry, cmd->text, cmd->color, 0x1A1A2E);
                    ry += 20;
                }
                break;
            case 2: /* Bold */
                for (int dx = 0; dx < 2; dx++)
                    vesa_draw_string(rx + dx, ry, cmd->text, cmd->color, 0x1A1A2E);
                ry += 18;
                break;
            case 3: /* Link */
                vesa_draw_string(rx, ry, cmd->text, cmd->color, 0x1A1A2E);
                /* Alt cizgi */
                for (int ux = 0; ux < k_strlen(cmd->text) * 8; ux++)
                    vesa_putpixel(rx + ux, ry + 14, cmd->color);
                ry += 18;
                break;
            case 4: /* HR */
                for (int hx = rx; hx < ww - 30; hx++)
                    vesa_putpixel(hx, ry + 4, cmd->color);
                ry += 12;
                break;
            case 5: /* List item */
                /* Bullet */
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        if (dx*dx + dy*dy <= 4)
                            vesa_putpixel(rx + 4 + dx, ry + 7 + dy, 0x0078D4);
                vesa_draw_string(rx + 14, ry, cmd->text, cmd->color, 0x1A1A2E);
                ry += 18;
                break;
            case 6: /* Blank line */
                ry += 8;
                break;
        }
    }

    /* Scroll bar */
    int total_h = 0;
    for (int i = 0; i < pg->cmd_count; i++) {
        if (pg->cmds[i].type == 1) total_h += (pg->cmds[i].level == 1 ? 28 : (pg->cmds[i].level == 2 ? 24 : 20));
        else if (pg->cmds[i].type == 4) total_h += 12;
        else if (pg->cmds[i].type == 6) total_h += 8;
        else total_h += 18;
    }
    if (total_h > content_h) {
        int sb_h = (content_h * content_h) / total_h;
        if (sb_h < 20) sb_h = 20;
        int sb_y = cy + (scroll_y * (content_h - sb_h)) / (total_h - content_h);
        vesa_fill_rect(ww - 8, cy, 8, content_h, 0x222240);
        vesa_fill_rect(ww - 6, sb_y, 6, sb_h, 0x555577);
    }

    /* Durum cubugu */
    int sty = sh - 24;
    vesa_fill_rect(wx, sty, ww, 24, 0x1F1F33);
    vesa_fill_rect(wx, sty, ww, 1, 0x333355);
    vesa_draw_string(wx + 8, sty + 4, pages[current_page].url, 0x888888, 0x1F1F33);
    vesa_draw_string(wx + ww - 180, sty + 4, "MyOS Browser v0.1", 0x555577, 0x1F1F33);
}

/* ======== ANA DONGU ======== */

void app_browser(void) {
    mouse_state_t ms;
    int sw = vesa_get_width(), sh = vesa_get_height();

    init_pages();

    /* Baslangic */
    current_page = 0;
    scroll_y = 0;
    hist_pos = 0;
    hist_count = 1;
    history[0] = 0;
    k_strcpy(addr_buf, pages[0].url);
    addr_len = k_strlen(addr_buf);
    addr_editing = 0;

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();

        /* Cikis */
        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;

        /* Adres cubugu editing */
        if (addr_editing) {
            if (key == '\n') {
                addr_buf[addr_len] = '\0';
                addr_editing = 0;
                navigate(addr_buf);
            } else if (key == '\b') {
                if (addr_len > 0) {
                    addr_len--;
                    addr_buf[addr_len] = '\0';
                }
            } else if (key >= 32 && key < 127 && addr_len < ADDR_MAX - 1) {
                addr_buf[addr_len++] = (char)key;
                addr_buf[addr_len] = '\0';
            }
        } else {
            if (key == '\b') go_back();
        }

        /* Scroll klavye */
        if (key == KEY_DOWN) scroll_y += 20;
        if (key == KEY_UP) {
            scroll_y -= 20;
            if (scroll_y < 0) scroll_y = 0;
        }

        /* Cizim */
        draw_browser(&ms);
        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        /* Tiklama */
        if (!ms.click) continue;
        mouse_clear_click();

        int ny = 30;

        /* Kapat */
        if (ms.x >= sw - 30 && ms.x < sw - 4 && ms.y >= 2 && ms.y < 28)
            return;

        /* Geri */
        if (ms.x >= 8 && ms.x < 36 && ms.y >= ny + 4 && ms.y < ny + 30)
            go_back();

        /* Ileri */
        if (ms.x >= 40 && ms.x < 68 && ms.y >= ny + 4 && ms.y < ny + 30)
            go_forward();

        /* Yenile */
        if (ms.x >= 72 && ms.x < 100 && ms.y >= ny + 4 && ms.y < ny + 30)
            scroll_y = 0;

        /* Ana sayfa */
        if (ms.x >= 104 && ms.x < 132 && ms.y >= ny + 4 && ms.y < ny + 30)
            navigate("myos://home");

        /* Adres cubugu tikla */
        if (ms.x >= 140 && ms.x < sw - 20 && ms.y >= ny + 5 && ms.y < ny + 31) {
            addr_editing = 1;
        } else {
            if (addr_editing) addr_editing = 0;
        }

        /* Yer imleri */
        int by2 = ny + 36;
        const char* burls[] = {"myos://home", "myos://about", "myos://help", "myos://news", "myos://demo"};
        for (int i = 0; i < 5; i++) {
            int bx = 8 + i * 100;
            if (ms.x >= bx && ms.x < bx + 90 && ms.y >= by2 + 2 && ms.y < by2 + 22) {
                navigate(burls[i]);
                break;
            }
        }

        /* Scroll butonlari / alan */
        if (ms.y > by2 + 24 && ms.y < sh - 24) {
            if (ms.x >= sw - 8) {
                /* Scroll bar tikla */
                int content_h = sh - by2 - 48;
                scroll_y = ((ms.y - by2 - 24) * 300) / content_h;
                if (scroll_y < 0) scroll_y = 0;
            }
        }
    }
}
