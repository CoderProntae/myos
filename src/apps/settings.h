#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *           MyOS Settings
 * ============================================
 * Sistem ayarları uygulaması
 * ============================================
 */

// ==================== SABITLER ====================

#define SETTINGS_MAX_CATEGORIES     10
#define SETTINGS_MAX_ITEMS          50

// ==================== KATEGORİLER ====================

typedef enum {
    SETTINGS_CAT_DISPLAY        = 0,    // Ekran
    SETTINGS_CAT_SOUND          = 1,    // Ses
    SETTINGS_CAT_NETWORK        = 2,    // Ağ
    SETTINGS_CAT_KEYBOARD       = 3,    // Klavye
    SETTINGS_CAT_MOUSE          = 4,    // Mouse
    SETTINGS_CAT_LANGUAGE       = 5,    // Dil
    SETTINGS_CAT_DATETIME       = 6,    // Tarih ve Saat
    SETTINGS_CAT_APPS           = 7,    // Uygulamalar
    SETTINGS_CAT_SYSTEM         = 8,    // Sistem
    SETTINGS_CAT_ABOUT          = 9,    // Hakkında
} SettingsCategory;

// ==================== AYAR TİPLERİ ====================

typedef enum {
    SETTING_TYPE_TOGGLE         = 0,    // On/Off
    SETTING_TYPE_SLIDER         = 1,    // Kaydırıcı (0-100)
    SETTING_TYPE_DROPDOWN       = 2,    // Seçim listesi
    SETTING_TYPE_TEXT           = 3,    // Metin girişi
    SETTING_TYPE_BUTTON         = 4,    // Aksiyon butonu
    SETTING_TYPE_COLOR          = 5,    // Renk seçici
    SETTING_TYPE_INFO           = 6,    // Sadece bilgi
} SettingType;

// ==================== AYAR ÖĞESİ ====================

typedef struct {
    char                name[64];
    char                description[128];
    SettingType         type;
    
    // Değerler
    union {
        bool            toggle_value;
        int32_t         slider_value;
        int32_t         dropdown_index;
        char            text_value[128];
        uint32_t        color_value;
    };
    
    // Dropdown için seçenekler
    char                options[10][32];
    int32_t             option_count;
    
    // Slider için min/max
    int32_t             min_value;
    int32_t             max_value;
    
    // Callback
    void                (*on_change)(void);
    
} SettingItem;

// ==================== KATEGORİ ====================

typedef struct {
    char                name[32];
    char                icon[8];        // Emoji/karakter
    SettingItem         items[20];
    int32_t             item_count;
} SettingsCategoryData;

// ==================== AYARLAR DURUMU ====================

typedef struct {
    // Kategoriler
    SettingsCategoryData    categories[SETTINGS_MAX_CATEGORIES];
    int32_t                 category_count;
    int32_t                 selected_category;
    int32_t                 selected_item;
    
    // UI
    bool                    editing;
    char                    edit_buffer[128];
    int32_t                 edit_cursor;
    
    // Pencere
    uint32_t                app_id;
    int32_t                 window_x, window_y;
    int32_t                 window_width, window_height;
    int32_t                 sidebar_width;
    
    // Scroll
    int32_t                 scroll_offset;
    
} SettingsState;

// ==================== SİSTEM AYARLARI (Global) ====================

typedef struct {
    // Ekran
    int32_t         brightness;         // 0-100
    int32_t         resolution_index;   // 0: 800x600, 1: 1024x768, 2: 1280x720
    bool            dark_mode;
    
    // Ses
    int32_t         volume;             // 0-100
    bool            mute;
    
    // Ağ
    bool            dhcp_enabled;
    char            ip_address[16];
    char            subnet_mask[16];
    char            gateway[16];
    char            dns_server[16];
    
    // Klavye
    int32_t         keyboard_layout;    // 0: US, 1: TR-Q, 2: TR-F, 3: DE, 4: FR
    int32_t         key_repeat_delay;   // ms
    int32_t         key_repeat_rate;    // chars/sec
    
    // Mouse
    int32_t         mouse_speed;        // 1-10
    bool            mouse_invert_y;
    bool            mouse_left_handed;
    
    // Dil
    int32_t         language;           // 0: English, 1: Türkçe, 2: Deutsch...
    
    // Tarih/Saat
    int32_t         timezone;           // UTC offset
    bool            time_24h;
    bool            auto_time;
    
    // Sistem
    char            hostname[32];
    char            username[32];
    bool            auto_login;
    
} SystemSettings;

// ==================== FONKSİYONLAR ====================

// Yaşam döngüsü
void                settings_init(void);
void                settings_update(void);
void                settings_render(void);
void                settings_handle_input(uint8_t key);
void                settings_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void                settings_cleanup(void);

// Kayıt
uint32_t            settings_register(void);

// Kategori yönetimi
void                settings_setup_categories(void);
void                settings_select_category(int32_t index);

// Ayar işlemleri
void                settings_save(void);
void                settings_load(void);
void                settings_reset_defaults(void);

// Değer değiştirme
void                settings_toggle_item(void);
void                settings_change_slider(int32_t delta);
void                settings_change_dropdown(int32_t delta);
void                settings_start_edit(void);
void                settings_end_edit(void);

// Callbacks (ayar değiştiğinde)
void                settings_on_brightness_change(void);
void                settings_on_resolution_change(void);
void                settings_on_dark_mode_change(void);
void                settings_on_volume_change(void);
void                settings_on_keyboard_layout_change(void);
void                settings_on_language_change(void);
void                settings_on_hostname_change(void);

// UI
void                settings_draw_sidebar(void);
void                settings_draw_content(void);
void                settings_draw_item(SettingItem* item, int32_t y, bool selected);

// ==================== GLOBAL ====================

extern SettingsState g_settings;
extern SystemSettings g_system_settings;

#endif
