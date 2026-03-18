#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ============================================
 *         MyOS Application Manager
 * ============================================
 * Tüm uygulamaları yöneten ana sistem
 * Versiyon: 1.0
 * ============================================
 */

// ==================== SABITLER ====================

#define MAX_APPS            32      // Maksimum uygulama sayısı
#define MAX_RUNNING_APPS    16      // Aynı anda çalışan maksimum uygulama
#define APP_NAME_LENGTH     64      // Uygulama adı maksimum uzunluk
#define APP_VERSION_LENGTH  16      // Versiyon string uzunluğu

// ==================== UYGULAMA DURUMLARI ====================

typedef enum {
    APP_STATE_INACTIVE      = 0,    // Çalışmıyor
    APP_STATE_STARTING      = 1,    // Başlatılıyor
    APP_STATE_RUNNING       = 2,    // Çalışıyor
    APP_STATE_PAUSED        = 3,    // Duraklatıldı
    APP_STATE_STOPPING      = 4,    // Durduruluyor
    APP_STATE_ERROR         = 5     // Hata
} AppState;

// ==================== UYGULAMA TİPLERİ ====================

typedef enum {
    APP_TYPE_SYSTEM         = 0,    // Sistem uygulaması (terminal, settings)
    APP_TYPE_UTILITY        = 1,    // Yardımcı (calculator, notepad)
    APP_TYPE_NETWORK        = 2,    // Ağ (browser)
    APP_TYPE_GAME           = 3,    // Oyun
    APP_TYPE_OTHER          = 4     // Diğer
} AppType;

// ==================== UYGULAMA YAPISI ====================

typedef struct {
    // Kimlik
    uint32_t        id;                             // Benzersiz ID
    char            name[APP_NAME_LENGTH];          // Uygulama adı
    char            version[APP_VERSION_LENGTH];    // Versiyon
    AppType         type;                           // Tip
    
    // Durum
    AppState        state;                          // Mevcut durum
    bool            visible;                        // Görünür mü?
    bool            focused;                        // Odakta mı?
    
    // Pencere bilgisi
    int32_t         window_id;                      // Bağlı pencere ID (-1 = yok)
    int32_t         x, y;                           // Konum
    int32_t         width, height;                  // Boyut
    
    // Fonksiyon pointer'ları
    void            (*init)(void);                  // Başlatma
    void            (*update)(void);                // Güncelleme (her frame)
    void            (*render)(void);                // Çizim
    void            (*handle_input)(uint8_t key);   // Klavye girişi
    void            (*handle_mouse)(int32_t x, int32_t y, uint8_t buttons);  // Mouse
    void            (*cleanup)(void);               // Temizlik
    
    // İstatistik
    uint32_t        start_time;                     // Başlama zamanı
    uint32_t        cpu_usage;                      // CPU kullanımı (%)
    uint32_t        memory_usage;                   // Bellek kullanımı (bytes)
    
} Application;

// ==================== UYGULAMA LİSTESİ ====================

typedef struct {
    Application     apps[MAX_APPS];                 // Kayıtlı uygulamalar
    uint32_t        count;                          // Toplam uygulama sayısı
    
    uint32_t        running[MAX_RUNNING_APPS];      // Çalışan uygulama ID'leri
    uint32_t        running_count;                  // Çalışan uygulama sayısı
    
    int32_t         focused_app;                    // Odaktaki uygulama ID (-1 = yok)
    
} AppManager;

// ==================== FONKSİYONLAR ====================

// Başlatma ve temizlik
void            app_manager_init(void);
void            app_manager_shutdown(void);

// Uygulama kaydı
uint32_t        app_register(const char* name, const char* version, AppType type);
bool            app_unregister(uint32_t app_id);

// Uygulama çalıştırma
bool            app_launch(uint32_t app_id);
bool            app_launch_by_name(const char* name);
bool            app_close(uint32_t app_id);
bool            app_close_all(void);

// Durum sorgulama
Application*    app_get(uint32_t app_id);
Application*    app_get_by_name(const char* name);
Application*    app_get_focused(void);
bool            app_is_running(uint32_t app_id);
uint32_t        app_get_running_count(void);

// Focus yönetimi
bool            app_set_focus(uint32_t app_id);
bool            app_focus_next(void);
bool            app_focus_prev(void);

// Güncelleme döngüsü
void            app_manager_update(void);
void            app_manager_render(void);

// Input dağıtımı
void            app_manager_handle_key(uint8_t key);
void            app_manager_handle_mouse(int32_t x, int32_t y, uint8_t buttons);

// Yardımcılar
const char*     app_state_to_string(AppState state);
const char*     app_type_to_string(AppType type);
void            app_manager_list_all(void);
void            app_manager_list_running(void);

// ==================== GLOBAL DEĞİŞKEN ====================

extern AppManager g_app_manager;

#endif
