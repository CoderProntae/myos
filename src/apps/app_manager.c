#include "app_manager.h"
#include "../kernel/kernel.h"
#include <string.h>

/*
 * ============================================
 *         MyOS Application Manager
 * ============================================
 * Uygulama yaşam döngüsü yönetimi
 * ============================================
 */

// ==================== GLOBAL ====================

AppManager g_app_manager;

// ==================== BAŞLATMA ====================

void app_manager_init(void) {
    // Tüm yapıyı sıfırla
    memset(&g_app_manager, 0, sizeof(AppManager));
    
    g_app_manager.count = 0;
    g_app_manager.running_count = 0;
    g_app_manager.focused_app = -1;
    
    // Tüm uygulamaları inactive yap
    for (int i = 0; i < MAX_APPS; i++) {
        g_app_manager.apps[i].id = 0;
        g_app_manager.apps[i].state = APP_STATE_INACTIVE;
        g_app_manager.apps[i].window_id = -1;
    }
    
    // Running listesini temizle
    for (int i = 0; i < MAX_RUNNING_APPS; i++) {
        g_app_manager.running[i] = 0;
    }
    
    print("[APP] Application Manager baslatildi.\n", 0x0A);
}

void app_manager_shutdown(void) {
    // Tüm çalışan uygulamaları kapat
    app_close_all();
    
    print("[APP] Application Manager kapatildi.\n", 0x0E);
}

// ==================== UYGULAMA KAYDI ====================

uint32_t app_register(const char* name, const char* version, AppType type) {
    // Boş slot bul
    int slot = -1;
    for (int i = 0; i < MAX_APPS; i++) {
        if (g_app_manager.apps[i].state == APP_STATE_INACTIVE && 
            g_app_manager.apps[i].id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        print("[APP] HATA: Maksimum uygulama sayisina ulasildi!\n", 0x0C);
        return 0;
    }
    
    // Uygulama bilgilerini doldur
    Application* app = &g_app_manager.apps[slot];
    
    app->id = slot + 1;  // ID 1'den başlar
    strncpy(app->name, name, APP_NAME_LENGTH - 1);
    app->name[APP_NAME_LENGTH - 1] = '\0';
    strncpy(app->version, version, APP_VERSION_LENGTH - 1);
    app->version[APP_VERSION_LENGTH - 1] = '\0';
    app->type = type;
    app->state = APP_STATE_INACTIVE;
    app->visible = false;
    app->focused = false;
    app->window_id = -1;
    app->x = 100;
    app->y = 100;
    app->width = 400;
    app->height = 300;
    
    // Fonksiyon pointer'larını NULL yap
    app->init = NULL;
    app->update = NULL;
    app->render = NULL;
    app->handle_input = NULL;
    app->handle_mouse = NULL;
    app->cleanup = NULL;
    
    // İstatistikler
    app->start_time = 0;
    app->cpu_usage = 0;
    app->memory_usage = 0;
    
    g_app_manager.count++;
    
    print("[APP] Uygulama kaydedildi: ", 0x0A);
    print(name, 0x0F);
    print(" v", 0x08);
    print(version, 0x08);
    print("\n", 0x0F);
    
    return app->id;
}

bool app_unregister(uint32_t app_id) {
    Application* app = app_get(app_id);
    if (!app) return false;
    
    // Çalışıyorsa önce kapat
    if (app->state == APP_STATE_RUNNING) {
        app_close(app_id);
    }
    
    // Sıfırla
    memset(app, 0, sizeof(Application));
    g_app_manager.count--;
    
    return true;
}

// ==================== UYGULAMA ÇALIŞTIRMA ====================

bool app_launch(uint32_t app_id) {
    Application* app = app_get(app_id);
    if (!app) {
        print("[APP] HATA: Uygulama bulunamadi!\n", 0x0C);
        return false;
    }
    
    // Zaten çalışıyor mu?
    if (app->state == APP_STATE_RUNNING) {
        // Sadece focus ver
        app_set_focus(app_id);
        return true;
    }
    
    // Çalışan uygulama limiti
    if (g_app_manager.running_count >= MAX_RUNNING_APPS) {
        print("[APP] HATA: Cok fazla uygulama calisiyor!\n", 0x0C);
        return false;
    }
    
    // Durumu güncelle
    app->state = APP_STATE_STARTING;
    
    // init fonksiyonunu çağır
    if (app->init) {
        app->init();
    }
    
    // Çalışan listeye ekle
    g_app_manager.running[g_app_manager.running_count] = app_id;
    g_app_manager.running_count++;
    
    // Durumu güncelle
    app->state = APP_STATE_RUNNING;
    app->visible = true;
    // app->start_time = get_ticks();  // Timer'dan al
    
    // Focus ver
    app_set_focus(app_id);
    
    print("[APP] Uygulama baslatildi: ", 0x0A);
    print(app->name, 0x0F);
    print("\n", 0x0F);
    
    return true;
}

bool app_launch_by_name(const char* name) {
    Application* app = app_get_by_name(name);
    if (!app) return false;
    return app_launch(app->id);
}

bool app_close(uint32_t app_id) {
    Application* app = app_get(app_id);
    if (!app) return false;
    
    if (app->state != APP_STATE_RUNNING) return false;
    
    app->state = APP_STATE_STOPPING;
    
    // cleanup fonksiyonunu çağır
    if (app->cleanup) {
        app->cleanup();
    }
    
    // Çalışan listeden çıkar
    for (uint32_t i = 0; i < g_app_manager.running_count; i++) {
        if (g_app_manager.running[i] == app_id) {
            // Kaydır
            for (uint32_t j = i; j < g_app_manager.running_count - 1; j++) {
                g_app_manager.running[j] = g_app_manager.running[j + 1];
            }
            g_app_manager.running_count--;
            break;
        }
    }
    
    // Durumu güncelle
    app->state = APP_STATE_INACTIVE;
    app->visible = false;
    app->focused = false;
    
    // Focus başka uygulamaya geç
    if (g_app_manager.focused_app == (int32_t)app_id) {
        if (g_app_manager.running_count > 0) {
            app_set_focus(g_app_manager.running[g_app_manager.running_count - 1]);
        } else {
            g_app_manager.focused_app = -1;
        }
    }
    
    print("[APP] Uygulama kapatildi: ", 0x0E);
    print(app->name, 0x0F);
    print("\n", 0x0F);
    
    return true;
}

bool app_close_all(void) {
    while (g_app_manager.running_count > 0) {
        app_close(g_app_manager.running[0]);
    }
    return true;
}

// ==================== DURUM SORGULAMA ====================

Application* app_get(uint32_t app_id) {
    if (app_id == 0 || app_id > MAX_APPS) return NULL;
    
    Application* app = &g_app_manager.apps[app_id - 1];
    if (app->id != app_id) return NULL;
    
    return app;
}

Application* app_get_by_name(const char* name) {
    for (int i = 0; i < MAX_APPS; i++) {
        if (g_app_manager.apps[i].id != 0) {
            if (strcmp(g_app_manager.apps[i].name, name) == 0) {
                return &g_app_manager.apps[i];
            }
        }
    }
    return NULL;
}

Application* app_get_focused(void) {
    if (g_app_manager.focused_app == -1) return NULL;
    return app_get(g_app_manager.focused_app);
}

bool app_is_running(uint32_t app_id) {
    Application* app = app_get(app_id);
    if (!app) return false;
    return app->state == APP_STATE_RUNNING;
}

uint32_t app_get_running_count(void) {
    return g_app_manager.running_count;
}

// ==================== FOCUS YÖNETİMİ ====================

bool app_set_focus(uint32_t app_id) {
    Application* app = app_get(app_id);
    if (!app) return false;
    if (app->state != APP_STATE_RUNNING) return false;
    
    // Eski focus'u kaldır
    if (g_app_manager.focused_app != -1) {
        Application* old_app = app_get(g_app_manager.focused_app);
        if (old_app) {
            old_app->focused = false;
        }
    }
    
    // Yeni focus
    app->focused = true;
    g_app_manager.focused_app = app_id;
    
    return true;
}

bool app_focus_next(void) {
    if (g_app_manager.running_count == 0) return false;
    
    // Mevcut index bul
    int current_idx = -1;
    for (uint32_t i = 0; i < g_app_manager.running_count; i++) {
        if (g_app_manager.running[i] == (uint32_t)g_app_manager.focused_app) {
            current_idx = i;
            break;
        }
    }
    
    // Sonraki
    int next_idx = (current_idx + 1) % g_app_manager.running_count;
    return app_set_focus(g_app_manager.running[next_idx]);
}

bool app_focus_prev(void) {
    if (g_app_manager.running_count == 0) return false;
    
    int current_idx = -1;
    for (uint32_t i = 0; i < g_app_manager.running_count; i++) {
        if (g_app_manager.running[i] == (uint32_t)g_app_manager.focused_app) {
            current_idx = i;
            break;
        }
    }
    
    int prev_idx = current_idx - 1;
    if (prev_idx < 0) prev_idx = g_app_manager.running_count - 1;
    
    return app_set_focus(g_app_manager.running[prev_idx]);
}

// ==================== GÜNCELLEME DÖNGÜSÜ ====================

void app_manager_update(void) {
    for (uint32_t i = 0; i < g_app_manager.running_count; i++) {
        Application* app = app_get(g_app_manager.running[i]);
        if (app && app->update) {
            app->update();
        }
    }
}

void app_manager_render(void) {
    for (uint32_t i = 0; i < g_app_manager.running_count; i++) {
        Application* app = app_get(g_app_manager.running[i]);
        if (app && app->visible && app->render) {
            app->render();
        }
    }
}

// ==================== INPUT DAĞITIMI ====================

void app_manager_handle_key(uint8_t key) {
    // Alt+Tab ile uygulama değiştirme (örnek)
    // if (alt_pressed && key == KEY_TAB) {
    //     app_focus_next();
    //     return;
    // }
    
    // Odaktaki uygulamaya gönder
    Application* focused = app_get_focused();
    if (focused && focused->handle_input) {
        focused->handle_input(key);
    }
}

void app_manager_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    // Hangi uygulamanın üzerinde?
    // TODO: Z-order'a göre kontrol et
    
    Application* focused = app_get_focused();
    if (focused && focused->handle_mouse) {
        focused->handle_mouse(x, y, buttons);
    }
}

// ==================== YARDIMCILAR ====================

const char* app_state_to_string(AppState state) {
    switch (state) {
        case APP_STATE_INACTIVE:    return "Inactive";
        case APP_STATE_STARTING:    return "Starting";
        case APP_STATE_RUNNING:     return "Running";
        case APP_STATE_PAUSED:      return "Paused";
        case APP_STATE_STOPPING:    return "Stopping";
        case APP_STATE_ERROR:       return "Error";
        default:                    return "Unknown";
    }
}

const char* app_type_to_string(AppType type) {
    switch (type) {
        case APP_TYPE_SYSTEM:       return "System";
        case APP_TYPE_UTILITY:      return "Utility";
        case APP_TYPE_NETWORK:      return "Network";
        case APP_TYPE_GAME:         return "Game";
        case APP_TYPE_OTHER:        return "Other";
        default:                    return "Unknown";
    }
}

void app_manager_list_all(void) {
    print("\n", 0x0F);
    print("  ====== Kayitli Uygulamalar ======\n", 0x0B);
    print("  ID   Ad                    Durum\n", 0x08);
    print("  --------------------------------\n", 0x08);
    
    for (int i = 0; i < MAX_APPS; i++) {
        Application* app = &g_app_manager.apps[i];
        if (app->id != 0) {
            // ID
            print("  ", 0x0F);
            // print_int(app->id);  // int yazdırma fonksiyonu gerekli
            print("    ", 0x0F);
            
            // Ad
            print(app->name, 0x0F);
            
            // Durum
            print("  [", 0x08);
            print(app_state_to_string(app->state), 
                  app->state == APP_STATE_RUNNING ? 0x0A : 0x07);
            print("]\n", 0x08);
        }
    }
    print("\n", 0x0F);
}

void app_manager_list_running(void) {
    print("\n", 0x0F);
    print("  ====== Calisan Uygulamalar ======\n", 0x0B);
    
    if (g_app_manager.running_count == 0) {
        print("  (Hicbir uygulama calismiyor)\n", 0x08);
    } else {
        for (uint32_t i = 0; i < g_app_manager.running_count; i++) {
            Application* app = app_get(g_app_manager.running[i]);
            if (app) {
                print("  ", 0x0F);
                if (app->focused) print("> ", 0x0E);
                else print("  ", 0x0F);
                print(app->name, app->focused ? 0x0E : 0x0F);
                print("\n", 0x0F);
            }
        }
    }
    print("\n", 0x0F);
}
