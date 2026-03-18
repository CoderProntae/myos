#include "browser.h"
#include "../kernel/kernel.h"
#include "../net/http.h"
#include "../net/dns.h"
#include <string.h>

/*
 * ============================================
 *           MyOS Web Browser
 * ============================================
 */

// ==================== GLOBAL ====================

BrowserState g_browser;

// ==================== RENKLER ====================

#define COLOR_BROWSER_BG        0x1E1E1E
#define COLOR_TOOLBAR_BG        0x2D2D2D
#define COLOR_TAB_BG            0x3C3C3C
#define COLOR_TAB_ACTIVE        0x1E1E1E
#define COLOR_URL_BG            0x404040
#define COLOR_URL_TEXT          0xFFFFFF
#define COLOR_CONTENT_BG        0xFFFFFF
#define COLOR_TEXT              0x000000
#define COLOR_LINK              0x0066CC
#define COLOR_LINK_VISITED      0x551A8B
#define COLOR_LINK_HOVER        0x0099FF
#define COLOR_HEADING           0x000000
#define COLOR_BUTTON            0x0078D4
#define COLOR_BUTTON_HOVER      0x1084D8
#define COLOR_SCROLLBAR         0x606060
#define COLOR_SCROLLBAR_THUMB   0x888888
#define COLOR_LOADING           0x0078D4
#define COLOR_ERROR             0xCC0000

// ==================== VARSAYILAN DEĞERLER ====================

#define DEFAULT_HOME_URL        "myos://home"
#define DEFAULT_FONT_SIZE       14
#define H1_FONT_SIZE            32
#define H2_FONT_SIZE            24
#define H3_FONT_SIZE            18
#define H4_FONT_SIZE            16
#define H5_FONT_SIZE            14
#define H6_FONT_SIZE            12

// ==================== YAŞAM DÖNGÜSÜ ====================

void browser_init(void) {
    memset(&g_browser, 0, sizeof(BrowserState));
    
    // Pencere boyutu
    g_browser.window_width = 800;
    g_browser.window_height = 600;
    g_browser.window_x = 50;
    g_browser.window_y = 50;
    
    // UI boyutları
    g_browser.tab_bar_height = 35;
    g_browser.toolbar_height = 45;
    g_browser.content_y = g_browser.tab_bar_height + g_browser.toolbar_height;
    
    // İlk sekme
    g_browser.tab_count = 0;
    g_browser.active_tab = 0;
    browser_new_tab();
    
    // Varsayılan URL
    strcpy(g_browser.url_bar, DEFAULT_HOME_URL);
    g_browser.url_focused = false;
    
    // Geçmiş
    g_browser.history_count = 0;
    g_browser.history_index = -1;
    
    // Yer imleri
    g_browser.bookmark_count = 0;
    
    // Varsayılan yer imleri ekle
    strcpy(g_browser.bookmarks[0].title, "MyOS Home");
    strcpy(g_browser.bookmarks[0].url, "myos://home");
    g_browser.bookmarks[0].used = true;
    
    strcpy(g_browser.bookmarks[1].title, "MyOS Hakkinda");
    strcpy(g_browser.bookmarks[1].url, "myos://about");
    g_browser.bookmarks[1].used = true;
    
    g_browser.bookmark_count = 2;
    
    // Ana sayfayı yükle
    browser_navigate(DEFAULT_HOME_URL);
}

void browser_update(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    
    // Yükleme animasyonu
    if (tab->page.loading) {
        // Spinner güncelle
    }
}

void browser_render(void) {
    // Arka plan
    // draw_filled_rect(g_browser.window_x, g_browser.window_y,
    //                  g_browser.window_width, g_browser.window_height,
    //                  COLOR_BROWSER_BG);
    
    // Sekme çubuğu
    browser_draw_tabs();
    
    // Araç çubuğu (geri, ileri, yenile, URL)
    browser_draw_toolbar();
    
    // Sayfa içeriği
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    if (tab->page.loading) {
        browser_draw_loading();
    } else if (tab->page.error) {
        browser_draw_error();
    } else if (tab->page.loaded) {
        browser_render_page(&tab->page);
    }
    
    // Scrollbar
    browser_draw_scrollbar();
    
    // Durum çubuğu
    browser_draw_status_bar();
    
    // Yer imleri paneli
    if (g_browser.show_bookmarks) {
        // browser_draw_bookmarks_panel();
    }
}

void browser_handle_input(uint8_t key) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    
    // URL çubuğu odakta mı?
    if (g_browser.url_focused) {
        switch (key) {
            case '\n':
            case '\r':
                // Enter - URL'ye git
                g_browser.url_focused = false;
                browser_navigate(g_browser.url_bar);
                return;
                
            case 27:  // Escape
                g_browser.url_focused = false;
                // URL'yi geri yükle
                strcpy(g_browser.url_bar, tab->page.url);
                return;
                
            case '\b':  // Backspace
                if (g_browser.url_cursor > 0) {
                    g_browser.url_cursor--;
                    memmove(&g_browser.url_bar[g_browser.url_cursor],
                            &g_browser.url_bar[g_browser.url_cursor + 1],
                            strlen(&g_browser.url_bar[g_browser.url_cursor + 1]) + 1);
                }
                return;
                
            default:
                // Karakter ekle
                if (key >= 32 && key < 127) {
                    uint32_t len = strlen(g_browser.url_bar);
                    if (len < BROWSER_MAX_URL - 1) {
                        memmove(&g_browser.url_bar[g_browser.url_cursor + 1],
                                &g_browser.url_bar[g_browser.url_cursor],
                                len - g_browser.url_cursor + 1);
                        g_browser.url_bar[g_browser.url_cursor] = key;
                        g_browser.url_cursor++;
                    }
                }
                return;
        }
    }
    
    // Genel kısayollar
    switch (key) {
        // Navigasyon
        case 'b':   // Alt+Left gibi davran
            browser_go_back();
            break;
        case 'f':   // Alt+Right gibi davran
            browser_go_forward();
            break;
        case 'r':   // Yenile
            browser_refresh();
            break;
        case 'h':   // Ana sayfa
            browser_go_home();
            break;
            
        // Sekme
        case 't':   // Yeni sekme (Ctrl+T gibi)
            browser_new_tab();
            break;
        case 'w':   // Sekme kapat (Ctrl+W gibi)
            browser_close_tab(g_browser.active_tab);
            break;
            
        // Scroll
        case 'j':   // Aşağı
            browser_scroll_down(50);
            break;
        case 'k':   // Yukarı
            browser_scroll_up(50);
            break;
        case ' ':   // Page Down
            browser_scroll_down(tab->page.scroll_max_y / 10);
            break;
            
        // Yer imi
        case 'd':   // Yer imi ekle (Ctrl+D gibi)
            browser_add_bookmark();
            break;
            
        // URL çubuğuna odaklan
        case 'l':   // Ctrl+L gibi
            g_browser.url_focused = true;
            g_browser.url_cursor = strlen(g_browser.url_bar);
            break;
            
        // Kapat
        case 27:    // Escape
            app_close(g_browser.app_id);
            break;
    }
}

void browser_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    int32_t local_x = x - g_browser.window_x;
    int32_t local_y = y - g_browser.window_y;
    
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    
    // Sekme çubuğu tıklama
    if (local_y < g_browser.tab_bar_height) {
        if (buttons & 1) {
            // Hangi sekmeye tıklandı?
            int tab_width = 150;
            int clicked_tab = local_x / tab_width;
            if (clicked_tab < (int)g_browser.tab_count) {
                browser_switch_tab(clicked_tab);
            }
            // Yeni sekme butonu (+)
            if (local_x > (int)g_browser.tab_count * tab_width && 
                local_x < (int)g_browser.tab_count * tab_width + 30) {
                browser_new_tab();
            }
        }
        return;
    }
    
    // Araç çubuğu tıklama
    if (local_y < g_browser.content_y) {
        if (buttons & 1) {
            // Buton konumları (yaklaşık)
            int btn_y = g_browser.tab_bar_height + 8;
            int btn_h = 30;
            
            // Geri butonu (x: 10-40)
            if (local_x >= 10 && local_x < 40 && local_y >= btn_y && local_y < btn_y + btn_h) {
                browser_go_back();
                return;
            }
            // İleri butonu (x: 45-75)
            if (local_x >= 45 && local_x < 75 && local_y >= btn_y && local_y < btn_y + btn_h) {
                browser_go_forward();
                return;
            }
            // Yenile butonu (x: 80-110)
            if (local_x >= 80 && local_x < 110 && local_y >= btn_y && local_y < btn_y + btn_h) {
                browser_refresh();
                return;
            }
            // Ana sayfa butonu (x: 115-145)
            if (local_x >= 115 && local_x < 145 && local_y >= btn_y && local_y < btn_y + btn_h) {
                browser_go_home();
                return;
            }
            // URL çubuğu (x: 155 - width-60)
            if (local_x >= 155 && local_x < g_browser.window_width - 60) {
                g_browser.url_focused = true;
                g_browser.url_cursor = strlen(g_browser.url_bar);
                return;
            }
            // Yer imi butonu (x: width-55 - width-25)
            if (local_x >= g_browser.window_width - 55 && local_x < g_browser.window_width - 25) {
                browser_add_bookmark();
                return;
            }
            // Menü butonu (x: width-25 - width)
            if (local_x >= g_browser.window_width - 25) {
                g_browser.show_menu = !g_browser.show_menu;
                return;
            }
        }
        return;
    }
    
    // Sayfa içeriği
    int32_t content_x = local_x;
    int32_t content_y = local_y - g_browser.content_y + tab->page.scroll_y;
    
    // Link hover kontrolü
    g_browser.hover_link_index = -1;
    for (uint32_t i = 0; i < tab->page.element_count; i++) {
        HTMLElement* el = &tab->page.elements[i];
        if (el->type == HTML_A && el->href[0] != '\0') {
            if (content_x >= el->x && content_x < el->x + el->width &&
                content_y >= el->y && content_y < el->y + el->height) {
                g_browser.hover_link_index = i;
                
                // Tıklama
                if (buttons & 1) {
                    browser_click_link(el);
                }
                break;
            }
        }
    }
    
    // Scroll (mouse wheel simülasyonu - sağ tık basılı sürükleme)
    if (buttons & 2) {
        static int32_t last_y = 0;
        if (last_y != 0) {
            int32_t delta = local_y - last_y;
            if (delta > 0) browser_scroll_down(delta * 3);
            else browser_scroll_up(-delta * 3);
        }
        last_y = local_y;
    } else {
        // last_y = 0;  // Static reset
    }
}

void browser_cleanup(void) {
    // Tüm sekmeleri temizle
    for (uint32_t i = 0; i < g_browser.tab_count; i++) {
        // DOM ağacını temizle
    }
}

// ==================== KAYIT ====================

uint32_t browser_register(void) {
    uint32_t id = app_register("Browser", "1.0", APP_TYPE_NETWORK);
    
    Application* app = app_get(id);
    if (app) {
        app->init = browser_init;
        app->update = browser_update;
        app->render = browser_render;
        app->handle_input = browser_handle_input;
        app->handle_mouse = browser_handle_mouse;
        app->cleanup = browser_cleanup;
        
        app->width = 800;
        app->height = 600;
    }
    
    g_browser.app_id = id;
    return id;
}

// ==================== NAVİGASYON ====================

void browser_navigate(const char* url) {
    if (!url || url[0] == '\0') return;
    
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    BrowserPage* page = &tab->page;
    
    // URL'yi kaydet
    strncpy(page->url, url, BROWSER_MAX_URL - 1);
    strncpy(g_browser.url_bar, url, BROWSER_MAX_URL - 1);
    
    // Durumu sıfırla
    page->loaded = false;
    page->loading = true;
    page->error = false;
    page->scroll_x = 0;
    page->scroll_y = 0;
    page->element_count = 0;
    
    // Özel URL'ler
    if (strncmp(url, "myos://", 7) == 0) {
        const char* special = url + 7;
        
        if (strcmp(special, "home") == 0) {
            browser_show_home_page();
        } else if (strcmp(special, "about") == 0) {
            browser_show_about_page();
        } else if (strcmp(special, "bookmarks") == 0) {
            browser_show_bookmarks();
        } else if (strcmp(special, "history") == 0) {
            browser_show_history();
        } else {
            browser_show_error_page("Bilinmeyen myos:// sayfasi");
        }
        return;
    }
    
    // HTTP isteği
    char response[BROWSER_PAGE_BUFFER];
    memset(response, 0, sizeof(response));
    
    if (browser_http_get(url, response, BROWSER_PAGE_BUFFER)) {
        strncpy(page->content, response, BROWSER_PAGE_BUFFER - 1);
        page->content_length = strlen(response);
        
        // HTML parse et
        browser_parse_html(page->content, page);
        
        page->loaded = true;
        page->loading = false;
        
        // Geçmişe ekle
        browser_add_to_history(url, page->title);
    } else {
        browser_show_error_page("Sayfa yuklenemedi");
    }
}

void browser_go_back(void) {
    if (g_browser.history_index > 0) {
        g_browser.history_index--;
        browser_navigate(g_browser.history[g_browser.history_index].url);
    }
}

void browser_go_forward(void) {
    if (g_browser.history_index < (int32_t)g_browser.history_count - 1) {
        g_browser.history_index++;
        browser_navigate(g_browser.history[g_browser.history_index].url);
    }
}

void browser_refresh(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    browser_navigate(tab->page.url);
}

void browser_stop(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    tab->page.loading = false;
}

void browser_go_home(void) {
    browser_navigate(DEFAULT_HOME_URL);
}

// ==================== SEKME YÖNETİMİ ====================

uint32_t browser_new_tab(void) {
    if (g_browser.tab_count >= BROWSER_MAX_TABS) {
        return UINT32_MAX;
    }
    
    BrowserTab* tab = &g_browser.tabs[g_browser.tab_count];
    memset(tab, 0, sizeof(BrowserTab));
    
    tab->used = true;
    tab->active = false;
    
    // Boş sayfa
    strcpy(tab->page.url, DEFAULT_HOME_URL);
    strcpy(tab->page.title, "Yeni Sekme");
    
    uint32_t new_index = g_browser.tab_count;
    g_browser.tab_count++;
    
    // Yeni sekmeye geç
    browser_switch_tab(new_index);
    
    return new_index;
}

void browser_close_tab(uint32_t tab_index) {
    if (tab_index >= g_browser.tab_count) return;
    if (g_browser.tab_count <= 1) {
        // Son sekme - tarayıcıyı kapat
        app_close(g_browser.app_id);
        return;
    }
    
    // Sekmeyi kaldır
    for (uint32_t i = tab_index; i < g_browser.tab_count - 1; i++) {
        g_browser.tabs[i] = g_browser.tabs[i + 1];
    }
    g_browser.tab_count--;
    
    // Aktif sekmeyi ayarla
    if (g_browser.active_tab >= g_browser.tab_count) {
        g_browser.active_tab = g_browser.tab_count - 1;
    }
    g_browser.tabs[g_browser.active_tab].active = true;
}

void browser_switch_tab(uint32_t tab_index) {
    if (tab_index >= g_browser.tab_count) return;
    
    // Eski aktifi pasif yap
    g_browser.tabs[g_browser.active_tab].active = false;
    
    // Yeni aktif
    g_browser.active_tab = tab_index;
    g_browser.tabs[tab_index].active = true;
    
    // URL çubuğunu güncelle
    strcpy(g_browser.url_bar, g_browser.tabs[tab_index].page.url);
}

void browser_next_tab(void) {
    uint32_t next = (g_browser.active_tab + 1) % g_browser.tab_count;
    browser_switch_tab(next);
}

void browser_prev_tab(void) {
    uint32_t prev = g_browser.active_tab == 0 ? g_browser.tab_count - 1 : g_browser.active_tab - 1;
    browser_switch_tab(prev);
}

// ==================== YER İMLERİ ====================

void browser_add_bookmark(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    
    // Zaten var mı?
    if (browser_is_bookmarked(tab->page.url)) {
        browser_remove_bookmark(tab->page.url);
        return;
    }
    
    if (g_browser.bookmark_count >= BROWSER_MAX_BOOKMARKS) return;
    
    Bookmark* bm = &g_browser.bookmarks[g_browser.bookmark_count];
    strncpy(bm->title, tab->page.title, 63);
    strncpy(bm->url, tab->page.url, BROWSER_MAX_URL - 1);
    bm->used = true;
    
    g_browser.bookmark_count++;
}

void browser_remove_bookmark(const char* url) {
    for (uint32_t i = 0; i < g_browser.bookmark_count; i++) {
        if (strcmp(g_browser.bookmarks[i].url, url) == 0) {
            // Kaldır
            for (uint32_t j = i; j < g_browser.bookmark_count - 1; j++) {
                g_browser.bookmarks[j] = g_browser.bookmarks[j + 1];
            }
            g_browser.bookmark_count--;
            return;
        }
    }
}

bool browser_is_bookmarked(const char* url) {
    for (uint32_t i = 0; i < g_browser.bookmark_count; i++) {
        if (strcmp(g_browser.bookmarks[i].url, url) == 0) {
            return true;
        }
    }
    return false;
}

void browser_show_bookmarks(void) {
    g_browser.show_bookmarks = !g_browser.show_bookmarks;
}

// ==================== GEÇMİŞ ====================

void browser_add_to_history(const char* url, const char* title) {
    // Aynı URL zaten son girişse ekleme
    if (g_browser.history_count > 0) {
        if (strcmp(g_browser.history[g_browser.history_count - 1].url, url) == 0) {
            return;
        }
    }
    
    // Yer aç
    if (g_browser.history_count >= BROWSER_MAX_HISTORY) {
        memmove(&g_browser.history[0], &g_browser.history[1],
                (BROWSER_MAX_HISTORY - 1) * sizeof(HistoryEntry));
        g_browser.history_count--;
    }
    
    // Ekle
    HistoryEntry* entry = &g_browser.history[g_browser.history_count];
    strncpy(entry->url, url, BROWSER_MAX_URL - 1);
    strncpy(entry->title, title, 63);
    entry->timestamp = 0;  // TODO: RTC'den al
    
    g_browser.history_count++;
    g_browser.history_index = g_browser.history_count - 1;
}

void browser_clear_history(void) {
    g_browser.history_count = 0;
    g_browser.history_index = -1;
}

void browser_show_history(void) {
    // Geçmiş sayfası göster
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    BrowserPage* page = &tab->page;
    
    strcpy(page->url, "myos://history");
    strcpy(page->title, "Gecmis");
    
    // HTML oluştur
    char html[4096] = "<html><head><title>Gecmis</title></head><body>";
    strcat(html, "<h1>Tarama Gecmisi</h1><hr>");
    
    if (g_browser.history_count == 0) {
        strcat(html, "<p>Gecmis bos.</p>");
    } else {
        strcat(html, "<ul>");
        for (int i = g_browser.history_count - 1; i >= 0; i--) {
            char item[512];
            // sprintf yerine manuel
            strcat(html, "<li><a href=\"");
            strcat(html, g_browser.history[i].url);
            strcat(html, "\">");
            strcat(html, g_browser.history[i].title);
            strcat(html, "</a></li>");
        }
        strcat(html, "</ul>");
    }
    strcat(html, "</body></html>");
    
    strcpy(page->content, html);
    page->content_length = strlen(html);
    
    browser_parse_html(page->content, page);
    
    page->loaded = true;
    page->loading = false;
}

// ==================== HTML PARSER ====================

HTMLElementType browser_tag_to_type(const char* tag) {
    if (strcmp(tag, "h1") == 0) return HTML_H1;
    if (strcmp(tag, "h2") == 0) return HTML_H2;
    if (strcmp(tag, "h3") == 0) return HTML_H3;
    if (strcmp(tag, "h4") == 0) return HTML_H4;
    if (strcmp(tag, "h5") == 0) return HTML_H5;
    if (strcmp(tag, "h6") == 0) return HTML_H6;
    if (strcmp(tag, "p") == 0) return HTML_P;
    if (strcmp(tag, "div") == 0) return HTML_DIV;
    if (strcmp(tag, "span") == 0) return HTML_SPAN;
    if (strcmp(tag, "a") == 0) return HTML_A;
    if (strcmp(tag, "img") == 0) return HTML_IMG;
    if (strcmp(tag, "br") == 0) return HTML_BR;
    if (strcmp(tag, "hr") == 0) return HTML_HR;
    if (strcmp(tag, "b") == 0 || strcmp(tag, "strong") == 0) return HTML_B;
    if (strcmp(tag, "i") == 0 || strcmp(tag, "em") == 0) return HTML_I;
    if (strcmp(tag, "u") == 0) return HTML_U;
    if (strcmp(tag, "ul") == 0) return HTML_UL;
    if (strcmp(tag, "ol") == 0) return HTML_OL;
    if (strcmp(tag, "li") == 0) return HTML_LI;
    if (strcmp(tag, "table") == 0) return HTML_TABLE;
    if (strcmp(tag, "tr") == 0) return HTML_TR;
    if (strcmp(tag, "td") == 0) return HTML_TD;
    if (strcmp(tag, "th") == 0) return HTML_TH;
    if (strcmp(tag, "form") == 0) return HTML_FORM;
    if (strcmp(tag, "input") == 0) return HTML_INPUT;
    if (strcmp(tag, "button") == 0) return HTML_BUTTON;
    if (strcmp(tag, "textarea") == 0) return HTML_TEXTAREA;
    if (strcmp(tag, "pre") == 0) return HTML_PRE;
    if (strcmp(tag, "code") == 0) return HTML_CODE;
    if (strcmp(tag, "blockquote") == 0) return HTML_BLOCKQUOTE;
    return HTML_UNKNOWN;
}

HTMLElement* browser_create_element(BrowserPage* page, HTMLElementType type) {
    if (page->element_count >= 256) return NULL;
    
    HTMLElement* el = &page->elements[page->element_count];
    memset(el, 0, sizeof(HTMLElement));
    
    el->type = type;
    el->color = COLOR_TEXT;
    el->bg_color = 0;
    el->font_size = DEFAULT_FONT_SIZE;
    
    page->element_count++;
    return el;
}

void browser_parse_html(const char* html, BrowserPage* page) {
    page->element_count = 0;
    page->dom_root = NULL;
    
    const char* p = html;
    char text_buffer[1024] = {0};
    int text_len = 0;
    
    bool in_tag = false;
    bool in_body = false;
    char tag_buffer[256] = {0};
    int tag_len = 0;
    
    // Stil stack
    bool bold_stack = false;
    bool italic_stack = false;
    bool underline_stack = false;
    char link_href[BROWSER_MAX_URL] = {0};
    int current_font_size = DEFAULT_FONT_SIZE;
    
    while (*p) {
        if (*p == '<') {
            // Önce birikmiş metni element olarak ekle
            if (text_len > 0 && in_body) {
                text_buffer[text_len] = '\0';
                
                HTMLElement* el = browser_create_element(page, HTML_TEXT);
                if (el) {
                    strcpy(el->text, text_buffer);
                    el->bold = bold_stack;
                    el->italic = italic_stack;
                    el->underline = underline_stack;
                    el->font_size = current_font_size;
                    
                    if (link_href[0] != '\0') {
                        el->type = HTML_A;
                        strcpy(el->href, link_href);
                        el->color = COLOR_LINK;
                    }
                }
                
                text_len = 0;
            }
            
            in_tag = true;
            tag_len = 0;
            p++;
            continue;
        }
        
        if (*p == '>') {
            tag_buffer[tag_len] = '\0';
            in_tag = false;
            
            // Tag'i parse et
            char tag_name[32] = {0};
            bool closing = false;
            
            const char* t = tag_buffer;
            
            // Kapanış tag'i mi?
            if (*t == '/') {
                closing = true;
                t++;
            }
            
            // Tag adını al
            int i = 0;
            while (*t && *t != ' ' && *t != '/' && i < 31) {
                tag_name[i++] = (*t >= 'A' && *t <= 'Z') ? *t + 32 : *t;  // lowercase
                t++;
            }
            tag_name[i] = '\0';
            
            // Özel tag'ler
            if (strcmp(tag_name, "body") == 0) {
                in_body = !closing;
            }
            else if (strcmp(tag_name, "title") == 0 && !closing) {
                // Title içeriğini al
                const char* title_start = p + 1;
                const char* title_end = strstr(title_start, "</title>");
                if (title_end) {
                    int title_len = title_end - title_start;
                    if (title_len > BROWSER_MAX_TITLE - 1) title_len = BROWSER_MAX_TITLE - 1;
                    strncpy(page->title, title_start, title_len);
                    page->title[title_len] = '\0';
                }
            }
            else if (in_body) {
                HTMLElementType type = browser_tag_to_type(tag_name);
                
                if (closing) {
                    // Kapanış tag'leri
                    if (type == HTML_B) bold_stack = false;
                    else if (type == HTML_I) italic_stack = false;
                    else if (type == HTML_U) underline_stack = false;
                    else if (type == HTML_A) link_href[0] = '\0';
                    else if (type >= HTML_H1 && type <= HTML_H6) current_font_size = DEFAULT_FONT_SIZE;
                }
                else {
                    // Açılış tag'leri
                    if (type == HTML_B) bold_stack = true;
                    else if (type == HTML_I) italic_stack = true;
                    else if (type == HTML_U) underline_stack = true;
                    else if (type == HTML_BR) {
                        HTMLElement* el = browser_create_element(page, HTML_BR);
                        (void)el;
                    }
                    else if (type == HTML_HR) {
                        HTMLElement* el = browser_create_element(page, HTML_HR);
                        (void)el;
                    }
                    else if (type == HTML_A) {
                        // href attribute'unu bul
                        const char* href = strstr(tag_buffer, "href=\"");
                        if (href) {
                            href += 6;
                            int j = 0;
                            while (*href && *href != '"' && j < BROWSER_MAX_URL - 1) {
                                link_href[j++] = *href++;
                            }
                            link_href[j] = '\0';
                        }
                    }
                    else if (type >= HTML_H1 && type <= HTML_H6) {
                        switch (type) {
                            case HTML_H1: current_font_size = H1_FONT_SIZE; break;
                            case HTML_H2: current_font_size = H2_FONT_SIZE; break;
                            case HTML_H3: current_font_size = H3_FONT_SIZE; break;
                            case HTML_H4: current_font_size = H4_FONT_SIZE; break;
                            case HTML_H5: current_font_size = H5_FONT_SIZE; break;
                            case HTML_H6: current_font_size = H6_FONT_SIZE; break;
                            default: break;
                        }
                        bold_stack = true;
                    }
                    else if (type == HTML_P || type == HTML_DIV) {
                        // Yeni satır
                        HTMLElement* el = browser_create_element(page, HTML_BR);
                        (void)el;
                    }
                    else if (type == HTML_LI) {
                        // Liste işareti
                        HTMLElement* el = browser_create_element(page, HTML_TEXT);
                        if (el) {
                            strcpy(el->text, "  • ");
                        }
                    }
                }
            }
            
            p++;
            continue;
        }
        
        if (in_tag) {
            if (tag_len < 255) {
                tag_buffer[tag_len++] = *p;
            }
        } else if (in_body) {
            // Metin karakteri
            // Whitespace normalize
            if (*p == '\n' || *p == '\r' || *p == '\t') {
                if (text_len > 0 && text_buffer[text_len - 1] != ' ') {
                    text_buffer[text_len++] = ' ';
                }
            } else {
                if (text_len < 1023) {
                    text_buffer[text_len++] = *p;
                }
            }
        }
        
        p++;
    }
    
    // Son metin
    if (text_len > 0 && in_body) {
        text_buffer[text_len] = '\0';
        HTMLElement* el = browser_create_element(page, HTML_TEXT);
        if (el) {
            strcpy(el->text, text_buffer);
        }
    }
}

// ==================== RENDER ====================

void browser_render_page(BrowserPage* page) {
    int32_t x = 10;
    int32_t y = g_browser.content_y + 10 - page->scroll_y;
    int32_t max_width = g_browser.window_width - 40;
    int32_t line_height = 20;
    
    for (uint32_t i = 0; i < page->element_count; i++) {
        HTMLElement* el = &page->elements[i];
        
        // Ekran dışındaysa atla
        if (y + line_height < g_browser.content_y || y > g_browser.window_height) {
            // Pozisyonu güncelle ama çizme
        }
        
        switch (el->type) {
            case HTML_TEXT:
            case HTML_A:
                // Metni çiz
                el->x = x;
                el->y = y;
                el->height = el->font_size + 4;
                el->width = strlen(el->text) * (el->font_size / 2);  // Yaklaşık
                
                // draw_text ile çiz
                // browser_render_text(el->text, g_browser.window_x + x, 
                //                     g_browser.window_y + y, el->color,
                //                     el->font_size, el->bold, el->italic);
                
                x += el->width;
                if (x > max_width) {
                    x = 10;
                    y += line_height;
                }
                break;
                
            case HTML_BR:
                x = 10;
                y += line_height;
                break;
                
            case HTML_HR:
                x = 10;
                y += 10;
                // draw_line(g_browser.window_x + 10, g_browser.window_y + y,
                //           g_browser.window_x + g_browser.window_width - 20,
                //           g_browser.window_y + y, COLOR_TEXT);
                y += 10;
                break;
                
            default:
                break;
        }
    }
    
    // Scroll max hesapla
    page->scroll_max_y = y + page->scroll_y - g_browser.window_height + 50;
    if (page->scroll_max_y < 0) page->scroll_max_y = 0;
}

// ==================== SCROLL ====================

void browser_scroll_up(int32_t amount) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    tab->page.scroll_y -= amount;
    if (tab->page.scroll_y < 0) tab->page.scroll_y = 0;
}

void browser_scroll_down(int32_t amount) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    tab->page.scroll_y += amount;
    if (tab->page.scroll_y > tab->page.scroll_max_y) {
        tab->page.scroll_y = tab->page.scroll_max_y;
    }
}

void browser_scroll_to(int32_t x, int32_t y) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    tab->page.scroll_x = x;
    tab->page.scroll_y = y;
}

// ==================== LINK ====================

void browser_click_link(HTMLElement* link) {
    if (!link || link->href[0] == '\0') return;
    
    // Relative URL kontrolü
    if (link->href[0] == '/') {
        // TODO: Base URL ile birleştir
    }
    
    browser_navigate(link->href);
}

// ==================== UI ÇİZİM ====================

void browser_draw_tabs(void) {
    int tab_y = g_browser.window_y;
    int tab_height = g_browser.tab_bar_height;
    int tab_width = 150;
    
    // Arka plan
    // draw_filled_rect(g_browser.window_x, tab_y,
    //                  g_browser.window_width, tab_height, COLOR_TOOLBAR_BG);
    
    for (uint32_t i = 0; i < g_browser.tab_count; i++) {
        int x = g_browser.window_x + i * tab_width;
        uint32_t bg = g_browser.tabs[i].active ? COLOR_TAB_ACTIVE : COLOR_TAB_BG;
        
        // draw_filled_rect(x, tab_y, tab_width - 2, tab_height, bg);
        
        // Tab başlığı
        char title[20];
        strncpy(title, g_browser.tabs[i].page.title, 15);
        title[15] = '\0';
        if (strlen(g_browser.tabs[i].page.title) > 15) {
            strcat(title, "...");
        }
        // draw_text(x + 10, tab_y + 10, title, COLOR_URL_TEXT);
        
        // Kapat butonu (x)
        // draw_text(x + tab_width - 20, tab_y + 10, "x", COLOR_URL_TEXT);
    }
    
    // Yeni sekme butonu (+)
    // draw_text(g_browser.window_x + g_browser.tab_count * tab_width + 10,
    //           tab_y + 10, "+", COLOR_URL_TEXT);
}

void browser_draw_toolbar(void) {
    int toolbar_y = g_browser.window_y + g_browser.tab_bar_height;
    
    // Arka plan
    // draw_filled_rect(g_browser.window_x, toolbar_y,
    //                  g_browser.window_width, g_browser.toolbar_height, COLOR_TOOLBAR_BG);
    
    int btn_x = g_browser.window_x + 10;
    int btn_y = toolbar_y + 8;
    int btn_size = 30;
    
    // Geri butonu
    // draw_button(btn_x, btn_y, btn_size, btn_size, "<");
    btn_x += btn_size + 5;
    
    // İleri butonu
    // draw_button(btn_x, btn_y, btn_size, btn_size, ">");
    btn_x += btn_size + 5;
    
    // Yenile butonu
    // draw_button(btn_x, btn_y, btn_size, btn_size, "R");
    btn_x += btn_size + 5;
    
    // Ana sayfa butonu
    // draw_button(btn_x, btn_y, btn_size, btn_size, "H");
    btn_x += btn_size + 10;
    
    // URL çubuğu
    int url_width = g_browser.window_width - btn_x - 70;
    // draw_filled_rect(btn_x, btn_y, url_width, btn_size, COLOR_URL_BG);
    // draw_text(btn_x + 5, btn_y + 8, g_browser.url_bar, COLOR_URL_TEXT);
    
    if (g_browser.url_focused) {
        // Cursor çiz
        // int cursor_x = btn_x + 5 + g_browser.url_cursor * 8;
        // draw_line(cursor_x, btn_y + 5, cursor_x, btn_y + btn_size - 5, COLOR_URL_TEXT);
    }
    
    // Yer imi butonu
    btn_x += url_width + 5;
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    const char* bm_icon = browser_is_bookmarked(tab->page.url) ? "*" : "☆";
    // draw_button(btn_x, btn_y, btn_size, btn_size, bm_icon);
    
    // Menü butonu
    btn_x += btn_size + 5;
    // draw_button(btn_x, btn_y, btn_size, btn_size, "≡");
}

void browser_draw_status_bar(void) {
    int status_y = g_browser.window_y + g_browser.window_height - 20;
    
    // draw_filled_rect(g_browser.window_x, status_y,
    //                  g_browser.window_width, 20, COLOR_TOOLBAR_BG);
    
    // Hover link göster
    if (g_browser.hover_link_index >= 0) {
        BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
        HTMLElement* el = &tab->page.elements[g_browser.hover_link_index];
        // draw_text(g_browser.window_x + 10, status_y + 4, el->href, COLOR_URL_TEXT);
    }
}

void browser_draw_scrollbar(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    if (tab->page.scroll_max_y <= 0) return;
    
    int sb_x = g_browser.window_x + g_browser.window_width - 15;
    int sb_y = g_browser.window_y + g_browser.content_y;
    int sb_height = g_browser.window_height - g_browser.content_y - 20;
    
    // Arka plan
    // draw_filled_rect(sb_x, sb_y, 15, sb_height, COLOR_SCROLLBAR);
    
    // Thumb
    float ratio = (float)tab->page.scroll_y / (float)tab->page.scroll_max_y;
    float visible_ratio = (float)sb_height / (float)(sb_height + tab->page.scroll_max_y);
    int thumb_height = (int)(sb_height * visible_ratio);
    if (thumb_height < 30) thumb_height = 30;
    int thumb_y = sb_y + (int)((sb_height - thumb_height) * ratio);
    
    // draw_filled_rect(sb_x + 2, thumb_y, 11, thumb_height, COLOR_SCROLLBAR_THUMB);
}

void browser_draw_loading(void) {
    int center_x = g_browser.window_x + g_browser.window_width / 2;
    int center_y = g_browser.window_y + g_browser.window_height / 2;
    
    // Spinner animasyonu
    // draw_text_centered(center_x, center_y, "Yukleniyor...", COLOR_LOADING);
}

void browser_draw_error(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    
    int center_x = g_browser.window_x + g_browser.window_width / 2;
    int center_y = g_browser.window_y + g_browser.window_height / 2;
    
    // draw_text_centered(center_x, center_y - 20, "Hata!", COLOR_ERROR);
    // draw_text_centered(center_x, center_y + 10, tab->page.error_message, COLOR_TEXT);
}

// ==================== VARSAYILAN SAYFALAR ====================

void browser_show_home_page(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    BrowserPage* page = &tab->page;
    
    strcpy(page->url, "myos://home");
    strcpy(page->title, "MyOS Browser - Ana Sayfa");
    
    const char* html = 
        "<html>"
        "<head><title>MyOS Browser - Ana Sayfa</title></head>"
        "<body>"
        "<h1>MyOS Web Browser</h1>"
        "<p>Hosgeldiniz! Bu MyOS isletim sisteminin yerlesik web tarayicisidir.</p>"
        "<hr>"
        "<h2>Hizli Erisim</h2>"
        "<ul>"
        "<li><a href=\"myos://about\">Tarayici Hakkinda</a></li>"
        "<li><a href=\"myos://bookmarks\">Yer Imleri</a></li>"
        "<li><a href=\"myos://history\">Gecmis</a></li>"
        "</ul>"
        "<hr>"
        "<h2>Test Siteleri</h2>"
        "<ul>"
        "<li><a href=\"http://example.com\">example.com</a></li>"
        "<li><a href=\"http://info.cern.ch\">CERN (ilk web sitesi)</a></li>"
        "</ul>"
        "<hr>"
        "<p><i>MyOS Browser v1.0</i></p>"
        "</body>"
        "</html>";
    
    strcpy(page->content, html);
    page->content_length = strlen(html);
    
    browser_parse_html(page->content, page);
    
    page->loaded = true;
    page->loading = false;
}

void browser_show_about_page(void) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    BrowserPage* page = &tab->page;
    
    strcpy(page->url, "myos://about");
    strcpy(page->title, "Hakkinda - MyOS Browser");
    
    const char* html = 
        "<html>"
        "<head><title>Hakkinda - MyOS Browser</title></head>"
        "<body>"
        "<h1>MyOS Web Browser</h1>"
        "<p><b>Versiyon:</b> 1.0</p>"
        "<p><b>Motor:</b> MyOS HTML Engine</p>"
        "<hr>"
        "<h2>Ozellikler</h2>"
        "<ul>"
        "<li>Temel HTML destegi (h1-h6, p, a, b, i, u, ul, ol, li, br, hr)</li>"
        "<li>HTTP/1.1 protokolu</li>"
        "<li>Sekmeli tarama</li>"
        "<li>Yer imleri</li>"
        "<li>Gecmis</li>"
        "</ul>"
        "<h2>Klavye Kisayollari</h2>"
        "<ul>"
        "<li><b>B</b> - Geri</li>"
        "<li><b>F</b> - Ileri</li>"
        "<li><b>R</b> - Yenile</li>"
        "<li><b>H</b> - Ana Sayfa</li>"
        "<li><b>T</b> - Yeni Sekme</li>"
        "<li><b>W</b> - Sekme Kapat</li>"
        "<li><b>D</b> - Yer Imi Ekle</li>"
        "<li><b>L</b> - URL Cubugu</li>"
        "<li><b>J/K</b> - Asagi/Yukari Kaydir</li>"
        "</ul>"
        "<hr>"
        "<p><a href=\"myos://home\">Ana Sayfaya Don</a></p>"
        "</body>"
        "</html>";
    
    strcpy(page->content, html);
    page->content_length = strlen(html);
    
    browser_parse_html(page->content, page);
    
    page->loaded = true;
    page->loading = false;
}

void browser_show_error_page(const char* error) {
    BrowserTab* tab = &g_browser.tabs[g_browser.active_tab];
    BrowserPage* page = &tab->page;
    
    page->error = true;
    strncpy(page->error_message, error, 127);
    page->loading = false;
    
    strcpy(page->title, "Hata");
    
    char html[1024];
    strcpy(html, "<html><head><title>Hata</title></head><body>");
    strcat(html, "<h1>Sayfa Yuklenemedi</h1>");
    strcat(html, "<p><b>Hata:</b> ");
    strcat(html, error);
    strcat(html, "</p>");
    strcat(html, "<hr>");
    strcat(html, "<p><a href=\"myos://home\">Ana Sayfaya Don</a></p>");
    strcat(html, "</body></html>");
    
    strcpy(page->content, html);
    browser_parse_html(page->content, page);
    page->loaded = true;
}

// ==================== HTTP ====================

bool browser_http_get(const char* url, char* response, uint32_t max_size) {
    // Basit HTTP GET isteği
    // Bu fonksiyon net.c/http.c ile entegre olacak
    
    char protocol[16], host[256], path[256];
    uint16_t port = 80;
    
    browser_parse_url(url, protocol, host, path, &port);
    
    // myos:// protokolü dahili işlenir
    if (strcmp(protocol, "myos") == 0) {
        return false;  // Dahili sayfalar navigate içinde işleniyor
    }
    
    // HTTP isteği yap
    // TODO: http_get(host, port, path, response, max_size);
    
    // Şimdilik test yanıtı
    strcpy(response, 
        "<html><body>"
        "<h1>Test Sayfasi</h1>"
        "<p>Bu bir test sayfasidir.</p>"
        "<p><a href=\"myos://home\">Ana Sayfa</a></p>"
        "</body></html>");
    
    return true;
}

bool browser_http_post(const char* url, const char* data, char* response, uint32_t max_size) {
    (void)url; (void)data; (void)response; (void)max_size;
    // TODO: HTTP POST implementasyonu
    return false;
}

void browser_parse_url(const char* url, char* protocol, char* host, char* path, uint16_t* port) {
    // Varsayılanlar
    strcpy(protocol, "http");
    strcpy(host, "");
    strcpy(path, "/");
    *port = 80;
    
    const char* p = url;
    
    // Protokol
    const char* proto_end = strstr(url, "://");
    if (proto_end) {
        int len = proto_end - url;
        if (len < 15) {
            strncpy(protocol, url, len);
            protocol[len] = '\0';
        }
        p = proto_end + 3;
    }
    
    // Host ve port
    const char* path_start = strchr(p, '/');
    const char* port_start = strchr(p, ':');
    
    if (port_start && (!path_start || port_start < path_start)) {
        // Port var
        int host_len = port_start - p;
        strncpy(host, p, host_len);
        host[host_len] = '\0';
        
        *port = 0;
        const char* pp = port_start + 1;
        while (*pp >= '0' && *pp <= '9') {
            *port = *port * 10 + (*pp - '0');
            pp++;
        }
    } else if (path_start) {
        int host_len = path_start - p;
        strncpy(host, p, host_len);
        host[host_len] = '\0';
    } else {
        strcpy(host, p);
    }
    
    // Path
    if (path_start) {
        strcpy(path, path_start);
    }
    
    // HTTPS portu
    if (strcmp(protocol, "https") == 0 && *port == 80) {
        *port = 443;
    }
}
