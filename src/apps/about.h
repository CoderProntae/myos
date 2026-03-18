#ifndef ABOUT_H
#define ABOUT_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *            MyOS About
 * ============================================
 * Sistem bilgisi penceresi
 * ============================================
 */

// ==================== HAKKINDA DURUMU ====================

typedef struct {
    uint32_t        app_id;
    int32_t         window_x, window_y;
    int32_t         window_width, window_height;
    
    // Sistem bilgileri
    char            os_name[64];
    char            os_version[32];
    char            kernel_version[32];
    char            build_date[32];
    
    char            cpu_info[64];
    uint32_t        total_memory;       // MB
    uint32_t        used_memory;        // MB
    uint32_t        free_memory;        // MB
    
    char            hostname[32];
    char            username[32];
    
    uint32_t        uptime_seconds;
    
    // Logo animasyon
    int32_t         logo_anim_frame;
    
} AboutState;

// ==================== FONKSİYONLAR ====================

// Yaşam döngüsü
void            about_init(void);
void            about_update(void);
void            about_render(void);
void            about_handle_input(uint8_t key);
void            about_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void            about_cleanup(void);

// Kayıt
uint32_t        about_register(void);

// Bilgi toplama
void            about_collect_info(void);
void            about_update_uptime(void);

// UI
void            about_draw_logo(void);
void            about_draw_info(void);
void            about_draw_buttons(void);

// ==================== GLOBAL ====================

extern AboutState g_about;

#endif
