#include "about.h"
#include "../kernel/kernel.h"
#include <string.h>

/*
 * ============================================
 *            MyOS About
 * ============================================
 */

// ==================== GLOBAL ====================

AboutState g_about;

// ==================== RENKLER ====================

#define COLOR_ABOUT_BG          0x1E1E1E
#define COLOR_LOGO              0x0078D4
#define COLOR_TEXT              0xCCCCCC
#define COLOR_TEXT_DIM          0x808080
#define COLOR_ACCENT            0x0078D4
#define COLOR_BUTTON            0x3C3C3C
#define COLOR_BUTTON_HOVER      0x505050

// ==================== YAŞAM DÖNGÜSÜ ====================

void about_init(void) {
    memset(&g_about, 0, sizeof(AboutState));
    
    // Pencere boyutu (küçük pencere)
    g_about.window_width = 450;
    g_about.window_height = 400;
    g_about.window_x = 200;
    g_about.window_y = 150;
    
    // Sistem bilgilerini topla
    about_collect_info();
    
    g_about.logo_anim_frame = 0;
}

void about_update(void) {
    // Logo animasyonu
    g_about.logo_anim_frame++;
    if (g_about.logo_anim_frame >= 60) {
        g_about.logo_anim_frame = 0;
    }
    
    // Uptime güncelle
    about_update_uptime();
}

void about_render(void) {
    // Arka plan
    // draw_filled_rect(g_about.window_x, g_about.window_y,
    //                  g_about.window_width, g_about.window_height,
    //                  COLOR_ABOUT_BG);
    
    // Logo
    about_draw_logo();
    
    // Bilgiler
    about_draw_info();
    
    // Butonlar
    about_draw_buttons();
}

void about_handle_input(uint8_t key) {
    switch (key) {
        case '\n':
        case '\r':
        case 27:    // Escape
        case ' ':
            app_close(g_about.app_id);
            break;
    }
}

void about_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    int32_t local_x = x - g_about.window_x;
    int32_t local_y = y - g_about.window_y;
    
    // Tamam butonu
    int btn_x = g_about.window_width / 2 - 50;
    int btn_y = g_about.window_height - 50;
    int btn_w = 100;
    int btn_h = 35;
    
    if (buttons & 1) {
        if (local_x >= btn_x && local_x < btn_x + btn_w &&
            local_y >= btn_y && local_y < btn_y + btn_h) {
            app_close(g_about.app_id);
        }
    }
}

void about_cleanup(void) {
    // Temizlik
}

// ==================== KAYIT ====================

uint32_t about_register(void) {
    uint32_t id = app_register("About", "1.0", APP_TYPE_SYSTEM);
    
    Application* app = app_get(id);
    if (app) {
        app->init = about_init;
        app->update = about_update;
        app->render = about_render;
        app->handle_input = about_handle_input;
        app->handle_mouse = about_handle_mouse;
        app->cleanup = about_cleanup;
        
        app->width = 450;
        app->height = 400;
    }
    
    g_about.app_id = id;
    return id;
}

// ==================== BİLGİ TOPLAMA ====================

void about_collect_info(void) {
    // OS bilgisi
    strcpy(g_about.os_name, "MyOS");
    strcpy(g_about.os_version, "0.3");
    strcpy(g_about.kernel_version, "0.3.0-x86");
    strcpy(g_about.build_date, "18 Mart 2026");
    
    // CPU bilgisi
    strcpy(g_about.cpu_info, "x86 Compatible Processor");
    
    // Bellek bilgisi (varsayılan/test)
    g_about.total_memory = 128;
    g_about.used_memory = 32;
    g_about.free_memory = 96;
    
    // TODO: Gerçek değerleri al
    // g_about.total_memory = get_total_memory() / (1024 * 1024);
    // g_about.used_memory = get_used_memory() / (1024 * 1024);
    // g_about.free_memory = g_about.total_memory - g_about.used_memory;
    
    // Kullanıcı bilgisi
    strcpy(g_about.hostname, "myos");
    strcpy(g_about.username, "user");
    
    // Uptime
    g_about.uptime_seconds = 0;
}

void about_update_uptime(void) {
    // TODO: Gerçek uptime'ı hesapla
    // g_about.uptime_seconds = get_ticks() / TICKS_PER_SECOND;
    
    // Test için her frame 1 saniye ekle (gerçekte olmaz)
    static uint32_t frame_count = 0;
    frame_count++;
    if (frame_count >= 60) {
        g_about.uptime_seconds++;
        frame_count = 0;
    }
}

// ==================== UI ÇİZİM ====================

void about_draw_logo(void) {
    int x = g_about.window_x + g_about.window_width / 2;
    int y = g_about.window_y + 50;
    
    // ASCII Logo
    const char* logo[] = {
        "  __  __        ___  ____  ",
        " |  \\/  |_   _ / _ \\/ ___| ",
        " | |\\/| | | | | | | \\___ \\ ",
        " | |  | | |_| | |_| |___) |",
        " |_|  |_|\\__, |\\___/|____/ ",
        "         |___/             ",
        NULL
    };
    
    int logo_y = y;
    for (int i = 0; logo[i]; i++) {
        // draw_text_centered(x, logo_y, logo[i], COLOR_LOGO);
        logo_y += 12;
    }
}

void about_draw_info(void) {
    int x = g_about.window_x + 30;
    int y = g_about.window_y + 140;
    int line_h = 22;
    
    // Versiyon
    // draw_text(x, y, "Versiyon:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, g_about.os_version, COLOR_TEXT);
    y += line_h;
    
    // Kernel
    // draw_text(x, y, "Kernel:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, g_about.kernel_version, COLOR_TEXT);
    y += line_h;
    
    // Build tarihi
    // draw_text(x, y, "Build Tarihi:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, g_about.build_date, COLOR_TEXT);
    y += line_h + 10;
    
    // Ayırıcı
    // draw_line(x, y, x + g_about.window_width - 60, y, COLOR_TEXT_DIM);
    y += 15;
    
    // CPU
    // draw_text(x, y, "Islemci:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, g_about.cpu_info, COLOR_TEXT);
    y += line_h;
    
    // Bellek
    char mem_str[64];
    // sprintf(mem_str, "%u MB / %u MB", g_about.used_memory, g_about.total_memory);
    strcpy(mem_str, "32 MB / 128 MB");
    // draw_text(x, y, "Bellek:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, mem_str, COLOR_TEXT);
    y += line_h;
    
    // Uptime
    uint32_t hours = g_about.uptime_seconds / 3600;
    uint32_t minutes = (g_about.uptime_seconds % 3600) / 60;
    uint32_t seconds = g_about.uptime_seconds % 60;
    
    char uptime_str[32];
    // sprintf(uptime_str, "%02u:%02u:%02u", hours, minutes, seconds);
    strcpy(uptime_str, "00:05:23");
    // draw_text(x, y, "Calisma Suresi:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, uptime_str, COLOR_TEXT);
    y += line_h + 10;
    
    // Ayırıcı
    // draw_line(x, y, x + g_about.window_width - 60, y, COLOR_TEXT_DIM);
    y += 15;
    
    // Kullanıcı
    // draw_text(x, y, "Kullanici:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, g_about.username, COLOR_TEXT);
    y += line_h;
    
    // Hostname
    // draw_text(x, y, "Bilgisayar:", COLOR_TEXT_DIM);
    // draw_text(x + 120, y, g_about.hostname, COLOR_TEXT);
}

void about_draw_buttons(void) {
    int btn_x = g_about.window_x + g_about.window_width / 2 - 50;
    int btn_y = g_about.window_y + g_about.window_height - 50;
    int btn_w = 100;
    int btn_h = 35;
    
    // Tamam butonu
    // draw_filled_rect(btn_x, btn_y, btn_w, btn_h, COLOR_ACCENT);
    // draw_text_centered(btn_x + btn_w/2, btn_y + 10, "Tamam", 0xFFFFFF);
}
