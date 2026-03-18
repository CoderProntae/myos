#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *         MyOS File Manager
 * ============================================
 * Dosya ve klasör yönetimi
 * ============================================
 */

// ==================== SABITLER ====================

#define FM_MAX_FILES            512     // Maksimum dosya/klasör
#define FM_MAX_PATH             512     // Maksimum yol uzunluğu
#define FM_MAX_NAME             256     // Dosya adı uzunluğu
#define FM_MAX_HISTORY          50      // Geçmiş yol sayısı

// ==================== DOSYA TİPLERİ ====================

typedef enum {
    FILE_TYPE_UNKNOWN       = 0,
    FILE_TYPE_FOLDER        = 1,
    FILE_TYPE_FILE          = 2,
    FILE_TYPE_TEXT          = 3,
    FILE_TYPE_IMAGE         = 4,
    FILE_TYPE_AUDIO         = 5,
    FILE_TYPE_VIDEO         = 6,
    FILE_TYPE_ARCHIVE       = 7,
    FILE_TYPE_EXECUTABLE    = 8,
    FILE_TYPE_DOCUMENT      = 9,
    FILE_TYPE_CODE          = 10,
} FileType;

// ==================== DOSYA GİRİŞİ ====================

typedef struct {
    char            name[FM_MAX_NAME];
    char            extension[16];
    FileType        type;
    uint64_t        size;               // Byte cinsinden
    uint32_t        permissions;        // rwx formatı
    uint32_t        created_time;
    uint32_t        modified_time;
    bool            hidden;
    bool            selected;
    bool            is_directory;
} FileEntry;

// ==================== GÖRÜNÜM MODU ====================

typedef enum {
    VIEW_MODE_LIST          = 0,        // Liste görünümü
    VIEW_MODE_ICONS         = 1,        // Simge görünümü
    VIEW_MODE_DETAILS       = 2,        // Detaylı liste
    VIEW_MODE_TILES         = 3,        // Döşeme görünümü
} ViewMode;

// ==================== SIRALAMA ====================

typedef enum {
    SORT_BY_NAME            = 0,
    SORT_BY_SIZE            = 1,
    SORT_BY_TYPE            = 2,
    SORT_BY_DATE            = 3,
} SortBy;

typedef enum {
    SORT_ORDER_ASC          = 0,
    SORT_ORDER_DESC         = 1,
} SortOrder;

// ==================== PANO İŞLEMİ ====================

typedef enum {
    CLIPBOARD_NONE          = 0,
    CLIPBOARD_CUT           = 1,
    CLIPBOARD_COPY          = 2,
} ClipboardMode;

// ==================== DOSYA YÖNETİCİSİ DURUMU ====================

typedef struct {
    // Mevcut dizin
    char            current_path[FM_MAX_PATH];
    FileEntry       files[FM_MAX_FILES];
    uint32_t        file_count;
    
    // Seçim
    int32_t         selected_index;
    uint32_t        selected_count;     // Çoklu seçim
    
    // Geçmiş
    char            history[FM_MAX_HISTORY][FM_MAX_PATH];
    int32_t         history_count;
    int32_t         history_index;
    
    // Pano
    char            clipboard_path[FM_MAX_PATH];
    char            clipboard_files[64][FM_MAX_NAME];
    uint32_t        clipboard_count;
    ClipboardMode   clipboard_mode;
    
    // Görünüm
    ViewMode        view_mode;
    SortBy          sort_by;
    SortOrder       sort_order;
    bool            show_hidden;
    
    // Arama
    bool            search_active;
    char            search_text[128];
    
    // Yeniden adlandırma
    bool            renaming;
    int32_t         rename_index;
    char            rename_buffer[FM_MAX_NAME];
    int32_t         rename_cursor;
    
    // Scroll
    int32_t         scroll_offset;
    int32_t         visible_items;
    
    // Pencere
    uint32_t        app_id;
    int32_t         window_x, window_y;
    int32_t         window_width, window_height;
    
    // UI boyutları
    int32_t         toolbar_height;
    int32_t         sidebar_width;
    int32_t         path_bar_height;
    int32_t         status_height;
    
    // Sidebar
    bool            show_sidebar;
    int32_t         sidebar_selected;   // Hızlı erişim
    
    // Sağ tık menü
    bool            context_menu_visible;
    int32_t         context_menu_x;
    int32_t         context_menu_y;
    
} FileManagerState;

// ==================== FONKSİYONLAR ====================

// Yaşam döngüsü
void            fm_init(void);
void            fm_update(void);
void            fm_render(void);
void            fm_handle_input(uint8_t key);
void            fm_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void            fm_cleanup(void);

// Kayıt
uint32_t        fm_register(void);

// Navigasyon
void            fm_navigate(const char* path);
void            fm_go_back(void);
void            fm_go_forward(void);
void            fm_go_up(void);
void            fm_go_home(void);
void            fm_refresh(void);

// Dosya işlemleri
void            fm_open_selected(void);
void            fm_create_folder(const char* name);
void            fm_create_file(const char* name);
void            fm_delete_selected(void);
void            fm_rename_start(void);
void            fm_rename_end(void);
void            fm_rename_cancel(void);

// Pano
void            fm_cut(void);
void            fm_copy(void);
void            fm_paste(void);

// Seçim
void            fm_select_item(int32_t index);
void            fm_select_all(void);
void            fm_deselect_all(void);
void            fm_toggle_selection(int32_t index);

// Görünüm
void            fm_set_view_mode(ViewMode mode);
void            fm_toggle_hidden(void);
void            fm_sort(SortBy by);
void            fm_toggle_sort_order(void);

// Arama
void            fm_search_start(void);
void            fm_search_end(void);
void            fm_search_execute(void);

// UI çizim
void            fm_draw_toolbar(void);
void            fm_draw_path_bar(void);
void            fm_draw_sidebar(void);
void            fm_draw_content(void);
void            fm_draw_file_entry(FileEntry* entry, int32_t x, int32_t y, bool selected);
void            fm_draw_status_bar(void);
void            fm_draw_context_menu(void);

// Yardımcı
void            fm_load_directory(const char* path);
void            fm_sort_files(void);
const char*     fm_get_file_icon(FileType type);
const char*     fm_format_size(uint64_t size);
FileType        fm_get_file_type(const char* name);
void            fm_add_to_history(const char* path);

// ==================== GLOBAL ====================

extern FileManagerState g_filemanager;

#endif
