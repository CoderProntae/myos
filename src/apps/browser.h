#ifndef BROWSER_H
#define BROWSER_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *           MyOS Web Browser
 * ============================================
 * Basit HTTP/HTML tarayıcı
 * ============================================
 */

// ==================== SABITLER ====================

#define BROWSER_MAX_URL         512     // Maksimum URL uzunluğu
#define BROWSER_MAX_TITLE       128     // Sayfa başlığı
#define BROWSER_MAX_HISTORY     50      // Geçmiş sayfa sayısı
#define BROWSER_MAX_BOOKMARKS   100     // Yer imi sayısı
#define BROWSER_MAX_TABS        10      // Maksimum sekme
#define BROWSER_PAGE_BUFFER     65536   // Sayfa içeriği buffer (64KB)
#define BROWSER_RENDER_BUFFER   32768   // Render buffer

// ==================== HTML ELEMENT TİPLERİ ====================

typedef enum {
    HTML_TEXT       = 0,
    HTML_H1         = 1,
    HTML_H2         = 2,
    HTML_H3         = 3,
    HTML_H4         = 4,
    HTML_H5         = 5,
    HTML_H6         = 6,
    HTML_P          = 7,
    HTML_DIV        = 8,
    HTML_SPAN       = 9,
    HTML_A          = 10,
    HTML_IMG        = 11,
    HTML_BR         = 12,
    HTML_HR         = 13,
    HTML_B          = 14,
    HTML_I          = 15,
    HTML_U          = 16,
    HTML_UL         = 17,
    HTML_OL         = 18,
    HTML_LI         = 19,
    HTML_TABLE      = 20,
    HTML_TR         = 21,
    HTML_TD         = 22,
    HTML_TH         = 23,
    HTML_FORM       = 24,
    HTML_INPUT      = 25,
    HTML_BUTTON     = 26,
    HTML_TEXTAREA   = 27,
    HTML_PRE        = 28,
    HTML_CODE       = 29,
    HTML_BLOCKQUOTE = 30,
    HTML_UNKNOWN    = 255
} HTMLElementType;

// ==================== HTML ELEMENT ====================

typedef struct HTMLElement {
    HTMLElementType         type;
    char                    tag[16];
    char                    text[1024];
    char                    href[BROWSER_MAX_URL];      // Link için
    char                    src[BROWSER_MAX_URL];       // Resim için
    char                    id[64];
    char                    class_name[64];
    
    // Stil
    uint32_t                color;
    uint32_t                bg_color;
    int32_t                 font_size;
    bool                    bold;
    bool                    italic;
    bool                    underline;
    
    // Pozisyon (render sonrası)
    int32_t                 x, y;
    int32_t                 width, height;
    
    // Ağaç yapısı
    struct HTMLElement*     parent;
    struct HTMLElement*     first_child;
    struct HTMLElement*     next_sibling;
    
} HTMLElement;

// ==================== SAYFA ====================

typedef struct {
    char                url[BROWSER_MAX_URL];
    char                title[BROWSER_MAX_TITLE];
    char                content[BROWSER_PAGE_BUFFER];
    uint32_t            content_length;
    
    HTMLElement*        dom_root;           // DOM ağacı kökü
    HTMLElement         elements[256];      // Element havuzu
    uint32_t            element_count;
    
    int32_t             scroll_x;
    int32_t             scroll_y;
    int32_t             scroll_max_x;
    int32_t             scroll_max_y;
    
    bool                loaded;
    bool                loading;
    bool                error;
    char                error_message[128];
    
} BrowserPage;

// ==================== SEKME ====================

typedef struct {
    BrowserPage         page;
    bool                active;
    bool                used;
} BrowserTab;

// ==================== YER İMİ ====================

typedef struct {
    char                title[64];
    char                url[BROWSER_MAX_URL];
    bool                used;
} Bookmark;

// ==================== GEÇMİŞ ====================

typedef struct {
    char                title[64];
    char                url[BROWSER_MAX_URL];
    uint32_t            timestamp;
} HistoryEntry;

// ==================== TARAYICI DURUMU ====================

typedef struct {
    // Sekmeler
    BrowserTab          tabs[BROWSER_MAX_TABS];
    uint32_t            tab_count;
    uint32_t            active_tab;
    
    // URL çubuğu
    char                url_bar[BROWSER_MAX_URL];
    uint32_t            url_cursor;
    bool                url_focused;
    
    // Geçmiş
    HistoryEntry        history[BROWSER_MAX_HISTORY];
    uint32_t            history_count;
    int32_t             history_index;      // Geri/ileri için
    
    // Yer imleri
    Bookmark            bookmarks[BROWSER_MAX_BOOKMARKS];
    uint32_t            bookmark_count;
    bool                show_bookmarks;
    
    // UI durumu
    bool                show_menu;
    bool                show_dev_tools;
    int32_t             hover_link_index;   // Mouse altındaki link
    
    // Pencere
    uint32_t            app_id;
    int32_t             window_x, window_y;
    int32_t             window_width, window_height;
    
    // Toolbar boyutları
    int32_t             toolbar_height;
    int32_t             tab_bar_height;
    int32_t             content_y;          // İçerik başlangıcı
    
    // Render
    uint32_t*           render_buffer;
    
} BrowserState;

// ==================== FONKSİYONLAR ====================

// Yaşam döngüsü
void                browser_init(void);
void                browser_update(void);
void                browser_render(void);
void                browser_handle_input(uint8_t key);
void                browser_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void                browser_cleanup(void);

// Kayıt
uint32_t            browser_register(void);

// Navigasyon
void                browser_navigate(const char* url);
void                browser_go_back(void);
void                browser_go_forward(void);
void                browser_refresh(void);
void                browser_stop(void);
void                browser_go_home(void);

// Sekme yönetimi
uint32_t            browser_new_tab(void);
void                browser_close_tab(uint32_t tab_index);
void                browser_switch_tab(uint32_t tab_index);
void                browser_next_tab(void);
void                browser_prev_tab(void);

// Yer imleri
void                browser_add_bookmark(void);
void                browser_remove_bookmark(const char* url);
bool                browser_is_bookmarked(const char* url);
void                browser_show_bookmarks(void);

// Geçmiş
void                browser_add_to_history(const char* url, const char* title);
void                browser_clear_history(void);
void                browser_show_history(void);

// URL işleme
void                browser_parse_url(const char* url, char* protocol, char* host, char* path, uint16_t* port);
void                browser_url_encode(const char* input, char* output);
void                browser_url_decode(const char* input, char* output);

// HTTP
bool                browser_http_get(const char* url, char* response, uint32_t max_size);
bool                browser_http_post(const char* url, const char* data, char* response, uint32_t max_size);

// HTML Parser
void                browser_parse_html(const char* html, BrowserPage* page);
HTMLElement*        browser_create_element(BrowserPage* page, HTMLElementType type);
void                browser_parse_tag(const char* tag, HTMLElement* element);
void                browser_parse_attributes(const char* attrs, HTMLElement* element);
HTMLElementType     browser_tag_to_type(const char* tag);

// Render
void                browser_render_page(BrowserPage* page);
void                browser_render_element(HTMLElement* element, int32_t* x, int32_t* y);
void                browser_render_text(const char* text, int32_t x, int32_t y, uint32_t color, int32_t font_size, bool bold, bool italic);

// Scroll
void                browser_scroll_up(int32_t amount);
void                browser_scroll_down(int32_t amount);
void                browser_scroll_to(int32_t x, int32_t y);

// Link
HTMLElement*        browser_find_link_at(int32_t x, int32_t y);
void                browser_click_link(HTMLElement* link);

// UI
void                browser_draw_toolbar(void);
void                browser_draw_tabs(void);
void                browser_draw_url_bar(void);
void                browser_draw_status_bar(void);
void                browser_draw_scrollbar(void);
void                browser_draw_loading(void);
void                browser_draw_error(void);

// Varsayılan sayfalar
void                browser_show_home_page(void);
void                browser_show_error_page(const char* error);
void                browser_show_about_page(void);

// ==================== GLOBAL ====================

extern BrowserState g_browser;

#endif
