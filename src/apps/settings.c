#include "settings.h"
#include "../kernel/kernel.h"
#include <string.h>

/*
 * ============================================
 *           MyOS Settings
 * ============================================
 */

// ==================== GLOBAL ====================

SettingsState g_settings;
SystemSettings g_system_settings;

// ==================== RENKLER ====================

#define COLOR_SETTINGS_BG       0x1E1E1E
#define COLOR_SIDEBAR_BG        0x252526
#define COLOR_SIDEBAR_SELECTED  0x37373D
#define COLOR_SIDEBAR_HOVER     0x2A2D2E
#define COLOR_CONTENT_BG        0x1E1E1E
#define COLOR_ITEM_BG           0x2D2D2D
#define COLOR_ITEM_SELECTED     0x094771
#define COLOR_TEXT              0xCCCCCC
#define COLOR_TEXT_DIM          0x808080
#define COLOR_ACCENT            0x0078D4
#define COLOR_TOGGLE_ON         0x0078D4
#define COLOR_TOGGLE_OFF        0x606060
#define COLOR_SLIDER_BG         0x3C3C3C
#define COLOR_SLIDER_FILL       0x0078D4

// ==================== YAŞAM DÖNGÜSÜ ====================

void settings_init(void) {
    memset(&g_settings, 0, sizeof(SettingsState));
    
    // Pencere boyutu
    g_settings.window_width = 700;
    g_settings.window_height = 500;
    g_settings.window_x = 80;
    g_settings.window_y = 80;
    g_settings.sidebar_width = 200;
    
    // Varsayılan sistem ayarları
    settings_load();
    
    // Kategorileri oluştur
    settings_setup_categories();
    
    g_settings.selected_category = 0;
    g_settings.selected_item = 0;
    g_settings.editing = false;
    g_settings.scroll_offset = 0;
}

void settings_update(void) {
    // Animasyonlar vs.
}

void settings_render(void) {
    // Arka plan
    // draw_filled_rect(g_settings.window_x, g_settings.window_y,
    //                  g_settings.window_width, g_settings.window_height,
    //                  COLOR_SETTINGS_BG);
    
    // Sidebar
    settings_draw_sidebar();
    
    // İçerik
    settings_draw_content();
}

void settings_handle_input(uint8_t key) {
    if (g_settings.editing) {
        // Metin düzenleme modu
        SettingItem* item = &g_settings.categories[g_settings.selected_category]
                             .items[g_settings.selected_item];
        
        switch (key) {
            case '\n':
            case '\r':
                settings_end_edit();
                if (item->on_change) item->on_change();
                break;
            case 27:  // Escape
                g_settings.editing = false;
                break;
            case '\b':
                if (g_settings.edit_cursor > 0) {
                    g_settings.edit_cursor--;
                    memmove(&g_settings.edit_buffer[g_settings.edit_cursor],
                            &g_settings.edit_buffer[g_settings.edit_cursor + 1],
                            strlen(&g_settings.edit_buffer[g_settings.edit_cursor + 1]) + 1);
                }
                break;
            default:
                if (key >= 32 && key < 127) {
                    int len = strlen(g_settings.edit_buffer);
                    if (len < 127) {
                        memmove(&g_settings.edit_buffer[g_settings.edit_cursor + 1],
                                &g_settings.edit_buffer[g_settings.edit_cursor],
                                len - g_settings.edit_cursor + 1);
                        g_settings.edit_buffer[g_settings.edit_cursor] = key;
                        g_settings.edit_cursor++;
                    }
                }
                break;
        }
        return;
    }
    
    SettingsCategoryData* cat = &g_settings.categories[g_settings.selected_category];
    SettingItem* item = &cat->items[g_settings.selected_item];
    
    switch (key) {
        // Navigasyon
        case 'k':   // Yukarı
            if (g_settings.selected_item > 0) {
                g_settings.selected_item--;
            }
            break;
            
        case 'j':   // Aşağı
            if (g_settings.selected_item < cat->item_count - 1) {
                g_settings.selected_item++;
            }
            break;
            
        case 'h':   // Sol - kategori yukarı
            if (g_settings.selected_category > 0) {
                g_settings.selected_category--;
                g_settings.selected_item = 0;
            }
            break;
            
        case 'l':   // Sağ - kategori aşağı
            if (g_settings.selected_category < g_settings.category_count - 1) {
                g_settings.selected_category++;
                g_settings.selected_item = 0;
            }
            break;
            
        // Değer değiştirme
        case '\n':
        case '\r':
        case ' ':
            switch (item->type) {
                case SETTING_TYPE_TOGGLE:
                    settings_toggle_item();
                    break;
                case SETTING_TYPE_TEXT:
                    settings_start_edit();
                    break;
                case SETTING_TYPE_BUTTON:
                    if (item->on_change) item->on_change();
                    break;
                default:
                    break;
            }
            break;
            
        case '-':
        case ',':
            if (item->type == SETTING_TYPE_SLIDER) {
                settings_change_slider(-5);
            } else if (item->type == SETTING_TYPE_DROPDOWN) {
                settings_change_dropdown(-1);
            }
            break;
            
        case '+':
        case '=':
        case '.':
            if (item->type == SETTING_TYPE_SLIDER) {
                settings_change_slider(5);
            } else if (item->type == SETTING_TYPE_DROPDOWN) {
                settings_change_dropdown(1);
            }
            break;
            
        // Kapat
        case 27:    // Escape
            settings_save();
            app_close(g_settings.app_id);
            break;
    }
}

void settings_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    int32_t local_x = x - g_settings.window_x;
    int32_t local_y = y - g_settings.window_y;
    
    // Sidebar tıklama
    if (local_x < g_settings.sidebar_width) {
        if (buttons & 1) {
            int item_height = 40;
            int clicked = (local_y - 10) / item_height;
            if (clicked >= 0 && clicked < g_settings.category_count) {
                g_settings.selected_category = clicked;
                g_settings.selected_item = 0;
            }
        }
        return;
    }
    
    // Content tıklama
    // TODO: Item tıklama
}

void settings_cleanup(void) {
    settings_save();
}

// ==================== KAYIT ====================

uint32_t settings_register(void) {
    uint32_t id = app_register("Settings", "1.0", APP_TYPE_SYSTEM);
    
    Application* app = app_get(id);
    if (app) {
        app->init = settings_init;
        app->update = settings_update;
        app->render = settings_render;
        app->handle_input = settings_handle_input;
        app->handle_mouse = settings_handle_mouse;
        app->cleanup = settings_cleanup;
        
        app->width = 700;
        app->height = 500;
    }
    
    g_settings.app_id = id;
    return id;
}

// ==================== KATEGORİ KURULUMU ====================

void settings_setup_categories(void) {
    g_settings.category_count = 0;
    
    // ===== EKRAN =====
    SettingsCategoryData* cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Ekran");
    strcpy(cat->icon, "🖥");
    cat->item_count = 0;
    
    SettingItem* item;
    
    // Parlaklık
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Parlaklik");
    strcpy(item->description, "Ekran parlakligi (0-100)");
    item->type = SETTING_TYPE_SLIDER;
    item->slider_value = g_system_settings.brightness;
    item->min_value = 0;
    item->max_value = 100;
    item->on_change = settings_on_brightness_change;
    
    // Çözünürlük
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Cozunurluk");
    strcpy(item->description, "Ekran cozunurlugu");
    item->type = SETTING_TYPE_DROPDOWN;
    item->dropdown_index = g_system_settings.resolution_index;
    strcpy(item->options[0], "800x600");
    strcpy(item->options[1], "1024x768");
    strcpy(item->options[2], "1280x720");
    item->option_count = 3;
    item->on_change = settings_on_resolution_change;
    
    // Koyu mod
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Koyu Mod");
    strcpy(item->description, "Koyu tema kullan");
    item->type = SETTING_TYPE_TOGGLE;
    item->toggle_value = g_system_settings.dark_mode;
    item->on_change = settings_on_dark_mode_change;
    
    // ===== SES =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Ses");
    strcpy(cat->icon, "🔊");
    cat->item_count = 0;
    
    // Ses seviyesi
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Ses Seviyesi");
    strcpy(item->description, "Ana ses seviyesi (0-100)");
    item->type = SETTING_TYPE_SLIDER;
    item->slider_value = g_system_settings.volume;
    item->min_value = 0;
    item->max_value = 100;
    item->on_change = settings_on_volume_change;
    
    // Sessiz
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Sessiz");
    strcpy(item->description, "Tum sesleri kapat");
    item->type = SETTING_TYPE_TOGGLE;
    item->toggle_value = g_system_settings.mute;
    
    // ===== KLAVYE =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Klavye");
    strcpy(cat->icon, "⌨");
    cat->item_count = 0;
    
    // Klavye düzeni
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Klavye Duzeni");
    strcpy(item->description, "Klavye karakter duzeni");
    item->type = SETTING_TYPE_DROPDOWN;
    item->dropdown_index = g_system_settings.keyboard_layout;
    strcpy(item->options[0], "US English");
    strcpy(item->options[1], "Turkce Q");
    strcpy(item->options[2], "Turkce F");
    strcpy(item->options[3], "Deutsch");
    strcpy(item->options[4], "Francais");
    item->option_count = 5;
    item->on_change = settings_on_keyboard_layout_change;
    
    // Tuş tekrar gecikmesi
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Tus Tekrar Gecikmesi");
    strcpy(item->description, "Tekrar baslamadan once bekleme (ms)");
    item->type = SETTING_TYPE_SLIDER;
    item->slider_value = g_system_settings.key_repeat_delay;
    item->min_value = 100;
    item->max_value = 1000;
    
    // ===== DİL =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Dil");
    strcpy(cat->icon, "🌐");
    cat->item_count = 0;
    
    // Sistem dili
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Sistem Dili");
    strcpy(item->description, "Arayuz dili");
    item->type = SETTING_TYPE_DROPDOWN;
    item->dropdown_index = g_system_settings.language;
    strcpy(item->options[0], "English");
    strcpy(item->options[1], "Turkce");
    strcpy(item->options[2], "Deutsch");
    strcpy(item->options[3], "Francais");
    strcpy(item->options[4], "Espanol");
    item->option_count = 5;
    item->on_change = settings_on_language_change;
    
    // ===== AĞ =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Ag");
    strcpy(cat->icon, "📶");
    cat->item_count = 0;
    
    // DHCP
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "DHCP");
    strcpy(item->description, "Otomatik IP adresi al");
    item->type = SETTING_TYPE_TOGGLE;
    item->toggle_value = g_system_settings.dhcp_enabled;
    
    // IP Adresi
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "IP Adresi");
    strcpy(item->description, "Manuel IP adresi");
    item->type = SETTING_TYPE_TEXT;
    strcpy(item->text_value, g_system_settings.ip_address);
    
    // DNS
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "DNS Sunucusu");
    strcpy(item->description, "DNS sunucu adresi");
    item->type = SETTING_TYPE_TEXT;
    strcpy(item->text_value, g_system_settings.dns_server);
    
    // ===== TARİH VE SAAT =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Tarih ve Saat");
    strcpy(cat->icon, "🕐");
    cat->item_count = 0;
    
    // 24 saat
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "24 Saat Formati");
    strcpy(item->description, "24 saat gosterimi kullan");
    item->type = SETTING_TYPE_TOGGLE;
    item->toggle_value = g_system_settings.time_24h;
    
    // Otomatik saat
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Otomatik Saat");
    strcpy(item->description, "Internet uzerinden saat ayarla");
    item->type = SETTING_TYPE_TOGGLE;
    item->toggle_value = g_system_settings.auto_time;
    
    // ===== SİSTEM =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Sistem");
    strcpy(cat->icon, "⚙");
    cat->item_count = 0;
    
    // Bilgisayar adı
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Bilgisayar Adi");
    strcpy(item->description, "Hostname");
    item->type = SETTING_TYPE_TEXT;
    strcpy(item->text_value, g_system_settings.hostname);
    item->on_change = settings_on_hostname_change;
    
    // Kullanıcı adı
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Kullanici Adi");
    strcpy(item->description, "Varsayilan kullanici");
    item->type = SETTING_TYPE_TEXT;
    strcpy(item->text_value, g_system_settings.username);
    
    // Varsayılanlara dön
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Varsayilanlara Don");
    strcpy(item->description, "Tum ayarlari sifirla");
    item->type = SETTING_TYPE_BUTTON;
    item->on_change = settings_reset_defaults;
    
    // ===== HAKKINDA =====
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Hakkinda");
    strcpy(cat->icon, "ℹ");
    cat->item_count = 0;
    
    // OS bilgisi
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Isletim Sistemi");
    strcpy(item->description, "MyOS v0.3");
    item->type = SETTING_TYPE_INFO;
    
    // Kernel
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Kernel");
    strcpy(item->description, "32-bit x86 Monolitik");
    item->type = SETTING_TYPE_INFO;
    
    // Bellek
    item = &cat->items[cat->item_count++];
    strcpy(item->name, "Bellek");
    strcpy(item->description, "128 MB RAM");
    item->type = SETTING_TYPE_INFO;
}

// ==================== AYAR İŞLEMLERİ ====================

void settings_save(void) {
    // Ayarları kalıcı olarak kaydet
    // TODO: Dosya sistemine yaz
}

void settings_load(void) {
    // Varsayılan değerler
    g_system_settings.brightness = 80;
    g_system_settings.resolution_index = 0;
    g_system_settings.dark_mode = true;
    
    g_system_settings.volume = 75;
    g_system_settings.mute = false;
    
    g_system_settings.dhcp_enabled = true;
    strcpy(g_system_settings.ip_address, "10.0.2.15");
    strcpy(g_system_settings.subnet_mask, "255.255.255.0");
    strcpy(g_system_settings.gateway, "10.0.2.2");
    strcpy(g_system_settings.dns_server, "8.8.8.8");
    
    g_system_settings.keyboard_layout = 1;  // Türkçe Q
    g_system_settings.key_repeat_delay = 500;
    g_system_settings.key_repeat_rate = 30;
    
    g_system_settings.mouse_speed = 5;
    g_system_settings.mouse_invert_y = false;
    g_system_settings.mouse_left_handed = false;
    
    g_system_settings.language = 1;  // Türkçe
    
    g_system_settings.timezone = 3;  // UTC+3
    g_system_settings.time_24h = true;
    g_system_settings.auto_time = true;
    
    strcpy(g_system_settings.hostname, "myos");
    strcpy(g_system_settings.username, "user");
    g_system_settings.auto_login = true;
    
    // TODO: Dosyadan oku
}

void settings_reset_defaults(void) {
    settings_load();
    settings_setup_categories();
}

// ==================== DEĞER DEĞİŞTİRME ====================

void settings_toggle_item(void) {
    SettingItem* item = &g_settings.categories[g_settings.selected_category]
                         .items[g_settings.selected_item];
    
    if (item->type != SETTING_TYPE_TOGGLE) return;
    
    item->toggle_value = !item->toggle_value;
    
    if (item->on_change) {
        item->on_change();
    }
}

void settings_change_slider(int32_t delta) {
    SettingItem* item = &g_settings.categories[g_settings.selected_category]
                         .items[g_settings.selected_item];
    
    if (item->type != SETTING_TYPE_SLIDER) return;
    
    item->slider_value += delta;
    if (item->slider_value < item->min_value) item->slider_value = item->min_value;
    if (item->slider_value > item->max_value) item->slider_value = item->max_value;
    
    if (item->on_change) {
        item->on_change();
    }
}

void settings_change_dropdown(int32_t delta) {
    SettingItem* item = &g_settings.categories[g_settings.selected_category]
                         .items[g_settings.selected_item];
    
    if (item->type != SETTING_TYPE_DROPDOWN) return;
    
    item->dropdown_index += delta;
    if (item->dropdown_index < 0) item->dropdown_index = 0;
    if (item->dropdown_index >= item->option_count) item->dropdown_index = item->option_count - 1;
    
    if (item->on_change) {
        item->on_change();
    }
}

void settings_start_edit(void) {
    SettingItem* item = &g_settings.categories[g_settings.selected_category]
                         .items[g_settings.selected_item];
    
    if (item->type != SETTING_TYPE_TEXT) return;
    
    g_settings.editing = true;
    strcpy(g_settings.edit_buffer, item->text_value);
    g_settings.edit_cursor = strlen(g_settings.edit_buffer);
}

void settings_end_edit(void) {
    SettingItem* item = &g_settings.categories[g_settings.selected_category]
                         .items[g_settings.selected_item];
    
    strcpy(item->text_value, g_settings.edit_buffer);
    g_settings.editing = false;
}

// ==================== CALLBACKS ====================

void settings_on_brightness_change(void) {
    // VESA brightness değiştir (varsa)
}

void settings_on_resolution_change(void) {
    // Çözünürlük değiştir (reboot gerekebilir)
}

void settings_on_dark_mode_change(void) {
    // Tema değiştir
}

void settings_on_volume_change(void) {
    // Ses seviyesi değiştir
}

void settings_on_keyboard_layout_change(void) {
    // Klavye düzenini değiştir
    SettingItem* item = &g_settings.categories[2].items[0];  // Klavye kategorisi
    g_system_settings.keyboard_layout = item->dropdown_index;
    // keyboard_set_layout(g_system_settings.keyboard_layout);
}

void settings_on_language_change(void) {
    // Dil değiştir (locale)
}

void settings_on_hostname_change(void) {
    SettingItem* item = &g_settings.categories[g_settings.selected_category]
                         .items[g_settings.selected_item];
    strcpy(g_system_settings.hostname, item->text_value);
}

// ==================== UI ÇİZİM ====================

void settings_draw_sidebar(void) {
    int x = g_settings.window_x;
    int y = g_settings.window_y;
    int w = g_settings.sidebar_width;
    int h = g_settings.window_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_SIDEBAR_BG);
    
    // Kategoriler
    int item_height = 40;
    for (int i = 0; i < g_settings.category_count; i++) {
        int item_y = y + 10 + i * item_height;
        uint32_t bg = (i == g_settings.selected_category) ? COLOR_SIDEBAR_SELECTED : COLOR_SIDEBAR_BG;
        
        // draw_filled_rect(x, item_y, w - 5, item_height - 2, bg);
        
        // İkon
        // draw_text(x + 15, item_y + 10, g_settings.categories[i].icon, COLOR_TEXT);
        
        // İsim
        // draw_text(x + 40, item_y + 12, g_settings.categories[i].name, COLOR_TEXT);
    }
}

void settings_draw_content(void) {
    int x = g_settings.window_x + g_settings.sidebar_width;
    int y = g_settings.window_y;
    int w = g_settings.window_width - g_settings.sidebar_width;
    int h = g_settings.window_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_CONTENT_BG);
    
    SettingsCategoryData* cat = &g_settings.categories[g_settings.selected_category];
    
    // Başlık
    // draw_text(x + 20, y + 20, cat->name, COLOR_TEXT);
    // draw_line(x + 20, y + 50, x + w - 20, y + 50, COLOR_TEXT_DIM);
    
    // Ayar öğeleri
    int item_y = y + 70;
    int item_height = 60;
    
    for (int i = 0; i < cat->item_count; i++) {
        bool selected = (i == g_settings.selected_item);
        settings_draw_item(&cat->items[i], item_y, selected);
        item_y += item_height;
    }
}

void settings_draw_item(SettingItem* item, int32_t y, bool selected) {
    int x = g_settings.window_x + g_settings.sidebar_width + 20;
    int w = g_settings.window_width - g_settings.sidebar_width - 40;
    int h = 55;
    
    uint32_t bg = selected ? COLOR_ITEM_SELECTED : COLOR_ITEM_BG;
    // draw_filled_rect(x, y, w, h, bg);
    
    // İsim
    // draw_text(x + 15, y + 10, item->name, COLOR_TEXT);
    
    // Açıklama
    // draw_text(x + 15, y + 30, item->description, COLOR_TEXT_DIM);
    
    // Değer (sağ tarafta)
    int value_x = x + w - 150;
    
    switch (item->type) {
        case SETTING_TYPE_TOGGLE: {
            // Toggle switch
            uint32_t toggle_color = item->toggle_value ? COLOR_TOGGLE_ON : COLOR_TOGGLE_OFF;
            // draw_filled_rect(value_x, y + 15, 50, 25, toggle_color);
            // draw_filled_circle(value_x + (item->toggle_value ? 35 : 15), y + 27, 10, 0xFFFFFF);
            break;
        }
        
        case SETTING_TYPE_SLIDER: {
            // Slider
            int slider_w = 120;
            int slider_h = 8;
            int slider_y = y + 20;
            
            // draw_filled_rect(value_x, slider_y, slider_w, slider_h, COLOR_SLIDER_BG);
            
            float ratio = (float)(item->slider_value - item->min_value) / 
                         (float)(item->max_value - item->min_value);
            int fill_w = (int)(slider_w * ratio);
            // draw_filled_rect(value_x, slider_y, fill_w, slider_h, COLOR_SLIDER_FILL);
            
            // Değer metni
            char val_str[16];
            // sprintf yerine itoa
            // draw_text(value_x + slider_w + 10, slider_y - 3, val_str, COLOR_TEXT);
            break;
        }
        
        case SETTING_TYPE_DROPDOWN: {
            // Dropdown
            // draw_filled_rect(value_x, y + 12, 130, 30, COLOR_SLIDER_BG);
            // draw_text(value_x + 10, y + 20, item->options[item->dropdown_index], COLOR_TEXT);
            // draw_text(value_x + 110, y + 20, "▼", COLOR_TEXT);
            break;
        }
        
        case SETTING_TYPE_TEXT: {
            // Metin kutusu
            // draw_filled_rect(value_x, y + 12, 130, 30, COLOR_SLIDER_BG);
            if (g_settings.editing && selected) {
                // draw_text(value_x + 10, y + 20, g_settings.edit_buffer, COLOR_TEXT);
                // Cursor çiz
            } else {
                // draw_text(value_x + 10, y + 20, item->text_value, COLOR_TEXT);
            }
            break;
        }
        
        case SETTING_TYPE_BUTTON: {
            // Buton
            // draw_filled_rect(value_x, y + 12, 100, 30, COLOR_ACCENT);
            // draw_text_centered(value_x + 50, y + 20, "Uygula", 0xFFFFFF);
            break;
        }
        
        case SETTING_TYPE_INFO: {
            // Sadece metin
            // draw_text(value_x, y + 20, item->description, COLOR_TEXT);
            break;
        }
        
        default:
            break;
    }
}
