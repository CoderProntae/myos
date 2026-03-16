#include "browser.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"
#include "http.h"
#include "html.h"
#include "net.h"

static char addr_buf[128];
static int addr_len = 0;
static int addr_editing = 0;
static int scroll_y = 0;

/* Sayfa verisi */
static html_doc_t current_doc;
static int page_loaded = 0;
static int loading = 0;
static char page_title[128];
static char status_text[128];

/* Onceden yuklu sayfalar */
static void load_builtin_home(void) {
    k_memset(&current_doc, 0, sizeof(current_doc));
    k_strcpy(page_title, "Ana Sayfa");

    html_node_t* n;
    #define ADD(t,l,txt,clr) n=&current_doc.nodes[current_doc.count++]; n->type=t; n->level=l; n->color=clr; if(txt) k_strcpy(n->text,txt); else n->text[0]=0; n->href[0]=0

    ADD(HTML_HEADING, 1, "MyOS Browser'a Hosgeldiniz!", 0x55CCFF);
    ADD(HTML_HR, 0, 0, 0x444466);
    ADD(HTML_NEWLINE, 0, 0, 0);
    ADD(HTML_TEXT, 0, "Gercek web sayfalari yukleyebilirsiniz!", 0xCCCCCC);
    ADD(HTML_TEXT, 0, "Adres cubuguna HTTP adresini yazin.", 0xAAAAAA);
    ADD(HTML_NEWLINE, 0, 0, 0);
    ADD(HTML_HEADING, 2, "Ornek Siteler (HTTP)", 0x55FF55);
    ADD(HTML_LIST, 0, "example.com", 0x5599FF);
    ADD(HTML_LIST, 0, "info.cern.ch (Ilk web sitesi!)", 0x5599FF);
    ADD(HTML_LIST, 0, "httpforever.com", 0x5599FF);
    ADD(HTML_NEWLINE, 0, 0, 0);
    ADD(HTML_HEADING, 2, "Kullanim", 0xFFFF55);
    ADD(HTML_LIST, 0, "Adres cubuguna tikla, site adi yaz, Enter bas", 0xCCCCCC);
    ADD(HTML_LIST, 0, "Sadece HTTP siteler (HTTPS desteklenmiyor)", 0xCCCCCC);
    ADD(HTML_LIST, 0, "ESC ile tarayiciyi kapat", 0xCCCCCC);
    #undef ADD

    page_loaded = 1;
    scroll_y = 0;
}

/* ======== Web sayfasi yukle ======== */

static void load_url(const char* url) {
    loading = 1;
    k_strcpy(status_text, "Yukleniyor...");

    /* URL parse: "host/path" veya "host" */
    char host[128] = {0};
    char path[128] = "/";

    /* "http://" atla */
    const char* p = url;
    if (k_strncmp(p, "http://", 7) == 0) p += 7;

    /* Host ve path ayir */
    int hi = 0;
    while (*p && *p != '/' && hi < 127) host[hi++] = *p++;
    host[hi] = '\0';
    if (*p == '/') {
        int pi = 0;
        while (*p && pi < 127) path[pi++] = *p++;
        path[pi] = '\0';
    }

    if (k_strlen(host) == 0) {
        k_strcpy(status_text, "Gecersiz adres");
        loading = 0;
        return;
    }

    /* HTTP GET */
    http_response_t resp;
    k_strcpy(status_text, "DNS cozumleniyor...");

    int ret = http_get(host, path, &resp);

    if (ret < 0) {
        k_strcpy(status_text, "Baglanti hatasi: ");
        char err[8];
        k_itoa(ret, err, 10);
        k_strcpy(status_text + k_strlen(status_text), err);
        loading = 0;

        /* Hata sayfasi goster */
        k_memset(&current_doc, 0, sizeof(current_doc));
        k_strcpy(page_title, "Hata");
        html_node_t* n;
        n = &current_doc.nodes[current_doc.count++];
        n->type = HTML_HEADING; n->level = 1; n->color = 0xFF5555;
        k_strcpy(n->text, "Sayfa Yuklenemedi");
        n = &current_doc.nodes[current_doc.count++];
        n->type = HTML_NEWLINE; n->level = 0; n->color = 0;
        n = &current_doc.nodes[current_doc.count++];
        n->type = HTML_TEXT; n->level = 0; n->color = 0xCCCCCC;
        k_strcpy(n->text, "Adres: ");
        k_strcpy(n->text + 7, host);
        n = &current_doc.nodes[current_doc.count++];
        n->type = HTML_TEXT; n->level = 0; n->color = 0xFF8844;
        k_strcpy(n->text, status_text);
        page_loaded = 1;
        return;
    }

    /* HTML parse */
    html_parse(resp.body, resp.body_len, &current_doc);

    /* Baslik */
    if (k_strlen(current_doc.title) > 0) {
        k_strcpy(page_title, current_doc.title);
    } else {
        k_strcpy(page_title, host);
    }

    char st[64];
    k_strcpy(st, "HTTP ");
    k_itoa(resp.status_code, st + 5, 10);
    k_strcpy(st + k_strlen(st), " | ");
    k_itoa(resp.body_len, st + k_strlen(st), 10);
    k_strcpy(st + k_strlen(st), " byte");
    k_strcpy(status_text, st);

    page_loaded = 1;
    loading = 0;
    scroll_y = 0;
}

/* ======== Cizim ======== */

static void draw_browser(mouse_state_t* ms) {
    int sw = vesa_get_width(), sh = vesa_get_height();

    vesa_fill_screen(0x1A1A2E);

    /* Baslik cubugu */
    vesa_fill_rect(0, 0, sw, 30, 0x1F1F33);

    /* Sekme */
    vesa_fill_rect(4, 2, 220, 26, 0x2B2B3D);
    vesa_draw_string(12, 8, page_title, 0xCCCCCC, 0x2B2B3D);

    /* Kapat */
    vesa_fill_rect(sw - 30, 2, 26, 26, COLOR_CLOSE_BTN);
    vesa_draw_string(sw - 22, 8, "X", COLOR_TEXT_WHITE, COLOR_CLOSE_BTN);

    /* Navigasyon */
    int ny = 30;
    vesa_fill_rect(0, ny, sw, 36, 0x252540);

    /* Ana sayfa butonu */
    int hb = (ms->x >= 8 && ms->x < 36 && ms->y >= ny+4 && ms->y < ny+30);
    vesa_fill_rect(8, ny+4, 28, 28, hb ? 0x3A3A5A : 0x252540);
    vesa_draw_string(14, ny+10, "H", 0xCCCCCC, hb ? 0x3A3A5A : 0x252540);

    /* 
