#include "filemanager.h"
#include "../kernel/kernel.h"
#include "../vfs/vfs.h"
#include <string.h>

/*
 * ============================================
 *         MyOS File Manager
 * ============================================
 */

// ==================== GLOBAL ====================

FileManagerState g_filemanager;

// ==================== RENKLER ====================

#define COLOR_FM_BG             0x1E1E1E
#define COLOR_TOOLBAR_BG        0x2D2D2D
#define COLOR_SIDEBAR_BG        0x252526
#define COLOR_CONTENT_BG        0x1E1E1E
#define COLOR_PATH_BG           0x3C3C3C
#define COLOR_ITEM_BG           0x2D2D2D
#define COLOR_ITEM_SELECTED     0x094771
#define COLOR_ITEM_HOVER        0x37373D
#define COLOR_TEXT              0xCCCCCC
#define COLOR_TEXT_DIM          0x808080
#define COLOR_FOLDER            0xDCB67A
#define COLOR_FILE              0xCCCCCC
#define COLOR_STATUS_BG         0x007ACC

// ==================== HIZLI ERİŞİM ====================

typedef struct {
    const char* name;
    const char* path;
    const char* icon;
} QuickAccess;

static QuickAccess quick_access[] = {
    {"Ana Dizin",   "/",            "🏠"},
    {"Belgeler",    "/home/docs",   "📄"},
    {"Resimler",    "/home/pics",   "🖼"},
    {"Muzik",       "/home/music",  "🎵"},
    {"Indirilenler","/home/down",   "📥"},
    {"Masaustu",    "/home/desktop","🖥"},
    {NULL, NULL, NULL}
};

// ==================== YAŞAM DÖNGÜSÜ ====================

void fm_init(void) {
    memset(&g_filemanager, 0, sizeof(FileManagerState));
    
    // Pencere boyutu
    g_filemanager.window_width = 800;
    g_filemanager.window_height = 550;
    g_filemanager.window_x = 60;
    g_filemanager.window_y = 60;
    
    // UI boyutları
    g_filemanager.toolbar_height = 40;
    g_filemanager.sidebar_width = 180;
    g_filemanager.path_bar_height = 35;
    g_filemanager.status_height = 25;
    g_filemanager.show_sidebar = true;
    
    // Görünüm ayarları
    g_filemanager.view_mode = VIEW_MODE_LIST;
    g_filemanager.sort_by = SORT_BY_NAME;
    g_filemanager.sort_order = SORT_ORDER_ASC;
    g_filemanager.show_hidden = false;
    
    // Başlangıç dizini
    strcpy(g_filemanager.current_path, "/");
    g_filemanager.history_count = 0;
    g_filemanager.history_index = -1;
    
    // Scroll
    g_filemanager.scroll_offset = 0;
    g_filemanager.visible_items = (g_filemanager.window_height - 
                                   g_filemanager.toolbar_height - 
                                   g_filemanager.path_bar_height - 
                                   g_filemanager.status_height) / 30;
    
    // İlk dizini yükle
    fm_navigate("/");
}

void fm_update(void) {
    // Animasyonlar
}

void fm_render(void) {
    // Arka plan
    // draw_filled_rect(g_filemanager.window_x, g_filemanager.window_y,
    //                  g_filemanager.window_width, g_filemanager.window_height,
    //                  COLOR_FM_BG);
    
    // Toolbar
    fm_draw_toolbar();
    
    // Yol çubuğu
    fm_draw_path_bar();
    
    // Sidebar
    if (g_filemanager.show_sidebar) {
        fm_draw_sidebar();
    }
    
    // İçerik
    fm_draw_content();
    
    // Durum çubuğu
    fm_draw_status_bar();
    
    // Sağ tık menü
    if (g_filemanager.context_menu_visible) {
        fm_draw_context_menu();
    }
}

void fm_handle_input(uint8_t key) {
    // Yeniden adlandırma modunda
    if (g_filemanager.renaming) {
        switch (key) {
            case '\n':
            case '\r':
                fm_rename_end();
                return;
            case 27:
                fm_rename_cancel();
                return;
            case '\b':
                if (g_filemanager.rename_cursor > 0) {
                    g_filemanager.rename_cursor--;
                    memmove(&g_filemanager.rename_buffer[g_filemanager.rename_cursor],
                            &g_filemanager.rename_buffer[g_filemanager.rename_cursor + 1],
                            strlen(&g_filemanager.rename_buffer[g_filemanager.rename_cursor + 1]) + 1);
                }
                return;
            default:
                if (key >= 32 && key < 127 && g_filemanager.rename_cursor < FM_MAX_NAME - 1) {
                    int len = strlen(g_filemanager.rename_buffer);
                    memmove(&g_filemanager.rename_buffer[g_filemanager.rename_cursor + 1],
                            &g_filemanager.rename_buffer[g_filemanager.rename_cursor],
                            len - g_filemanager.rename_cursor + 1);
                    g_filemanager.rename_buffer[g_filemanager.rename_cursor++] = key;
                }
                return;
        }
    }
    
    // Arama modunda
    if (g_filemanager.search_active) {
        switch (key) {
            case 27:
                fm_search_end();
                return;
            case '\n':
            case '\r':
                fm_search_execute();
                return;
            case '\b':
                if (strlen(g_filemanager.search_text) > 0) {
                    g_filemanager.search_text[strlen(g_filemanager.search_text) - 1] = '\0';
                }
                return;
            default:
                if (key >= 32 && key < 127 && strlen(g_filemanager.search_text) < 127) {
                    int len = strlen(g_filemanager.search_text);
                    g_filemanager.search_text[len] = key;
                    g_filemanager.search_text[len + 1] = '\0';
                }
                return;
        }
    }
    
    // Normal mod
    switch (key) {
        // Navigasyon
        case 'k':   // Yukarı
            if (g_filemanager.selected_index > 0) {
                g_filemanager.selected_index--;
                fm_deselect_all();
                g_filemanager.files[g_filemanager.selected_index].selected = true;
                g_filemanager.selected_count = 1;
                
                // Scroll ayarla
                if (g_filemanager.selected_index < g_filemanager.scroll_offset) {
                    g_filemanager.scroll_offset = g_filemanager.selected_index;
                }
            }
            break;
            
        case 'j':   // Aşağı
            if (g_filemanager.selected_index < (int32_t)g_filemanager.file_count - 1) {
                g_filemanager.selected_index++;
                fm_deselect_all();
                g_filemanager.files[g_filemanager.selected_index].selected = true;
                g_filemanager.selected_count = 1;
                
                // Scroll ayarla
                if (g_filemanager.selected_index >= g_filemanager.scroll_offset + g_filemanager.visible_items) {
                    g_filemanager.scroll_offset = g_filemanager.selected_index - g_filemanager.visible_items + 1;
                }
            }
            break;
            
        case 'h':   // Üst dizin
            fm_go_up();
            break;
            
        case 'l':   // Aç
        case '\n':
        case '\r':
            fm_open_selected();
            break;
            
        case '\b':  // Geri
            fm_go_back();
            break;
            
        // İşlemler
        case 'n':   // Yeni klasör
            fm_create_folder("Yeni Klasor");
            break;
            
        case 'r':   // Yeniden adlandır
            fm_rename_start();
            break;
            
        case 'd':   // Sil
        case 127:   // Delete
            fm_delete_selected();
            break;
            
        case 3:     // Ctrl+C - Kopyala
            fm_copy();
            break;
            
        case 24:    // Ctrl+X - Kes
            fm_cut();
            break;
            
        case 22:    // Ctrl+V - Yapıştır
            fm_paste();
            break;
            
        case 1:     // Ctrl+A - Tümünü seç
            fm_select_all();
            break;
            
        case 6:     // Ctrl+F - Ara
            fm_search_start();
            break;
            
        // Görünüm
        case '1':
            fm_set_view_mode(VIEW_MODE_LIST);
            break;
        case '2':
            fm_set_view_mode(VIEW_MODE_ICONS);
            break;
        case '3':
            fm_set_view_mode(VIEW_MODE_DETAILS);
            break;
            
        case '.':   // Gizli dosyalar
            fm_toggle_hidden();
            break;
            
        case 5:     // Ctrl+E veya F5 - Yenile
            fm_refresh();
            break;
            
        // Kapat
        case 27:
            if (g_filemanager.context_menu_visible) {
                g_filemanager.context_menu_visible = false;
            } else {
                app_close(g_filemanager.app_id);
            }
            break;
    }
}

void fm_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    int32_t local_x = x - g_filemanager.window_x;
    int32_t local_y = y - g_filemanager.window_y;
    
    // Sağ tık menüsü açıksa
    if (g_filemanager.context_menu_visible) {
        if (buttons & 1) {
            // Menü öğesine tıklama
            // TODO: Menü işlemleri
            g_filemanager.context_menu_visible = false;
        }
        return;
    }
    
    // Toolbar
    if (local_y < g_filemanager.toolbar_height) {
        if (buttons & 1) {
            int btn_x = 10;
            int btn_w = 35;
            int gap = 5;
            
            // Geri
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                fm_go_back();
                return;
            }
            btn_x += btn_w + gap;
            
            // İleri
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                fm_go_forward();
                return;
            }
            btn_x += btn_w + gap;
            
            // Yukarı
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                fm_go_up();
                return;
            }
            btn_x += btn_w + gap + 10;
            
            // Yenile
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                fm_refresh();
                return;
            }
            btn_x += btn_w + gap;
            
            // Ana dizin
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                fm_go_home();
                return;
            }
        }
        return;
    }
    
    // Sidebar
    if (g_filemanager.show_sidebar && local_x < g_filemanager.sidebar_width) {
        if (buttons & 1) {
            int item_h = 35;
            int header_h = 30;
            int clicked = (local_y - g_filemanager.toolbar_height - 
                          g_filemanager.path_bar_height - header_h) / item_h;
            
            if (clicked >= 0 && quick_access[clicked].path != NULL) {
                fm_navigate(quick_access[clicked].path);
            }
        }
        return;
    }
    
    // İçerik alanı
    int content_x = g_filemanager.show_sidebar ? g_filemanager.sidebar_width : 0;
    int content_y = g_filemanager.toolbar_height + g_filemanager.path_bar_height;
    int content_h = g_filemanager.window_height - content_y - g_filemanager.status_height;
    
    if (local_x >= content_x && local_y >= content_y && 
        local_y < content_y + content_h) {
        
        int item_h = 30;
        int clicked_index = g_filemanager.scroll_offset + 
                           (local_y - content_y) / item_h;
        
        if (clicked_index >= 0 && clicked_index < (int32_t)g_filemanager.file_count) {
            if (buttons & 1) {
                // Sol tık
                g_filemanager.selected_index = clicked_index;
                fm_deselect_all();
                g_filemanager.files[clicked_index].selected = true;
                g_filemanager.selected_count = 1;
            }
            else if (buttons & 2) {
                // Sağ tık - context menü
                g_filemanager.selected_index = clicked_index;
                g_filemanager.files[clicked_index].selected = true;
                
                g_filemanager.context_menu_visible = true;
                g_filemanager.context_menu_x = local_x;
                g_filemanager.context_menu_y = local_y;
            }
        }
        
        // Çift tıklama (basit simülasyon)
        static uint32_t last_click_time = 0;
        static int32_t last_click_index = -1;
        
        if (buttons & 1) {
            // uint32_t current_time = get_ticks();
            if (clicked_index == last_click_index /* && current_time - last_click_time < 500 */) {
                // Çift tıklama - aç
                fm_open_selected();
            }
            // last_click_time = current_time;
            last_click_index = clicked_index;
        }
    }
}

void fm_cleanup(void) {
    // Temizlik
}

// ==================== KAYIT ====================

uint32_t fm_register(void) {
    uint32_t id = app_register("Files", "1.0", APP_TYPE_SYSTEM);
    
    Application* app = app_get(id);
    if (app) {
        app->init = fm_init;
        app->update = fm_update;
        app->render = fm_render;
        app->handle_input = fm_handle_input;
        app->handle_mouse = fm_handle_mouse;
        app->cleanup = fm_cleanup;
        
        app->width = 800;
        app->height = 550;
    }
    
    g_filemanager.app_id = id;
    return id;
}

// ==================== NAVİGASYON ====================

void fm_navigate(const char* path) {
    fm_add_to_history(path);
    strncpy(g_filemanager.current_path, path, FM_MAX_PATH - 1);
    g_filemanager.scroll_offset = 0;
    g_filemanager.selected_index = 0;
    fm_load_directory(path);
}

void fm_go_back(void) {
    if (g_filemanager.history_index > 0) {
        g_filemanager.history_index--;
        strncpy(g_filemanager.current_path, 
                g_filemanager.history[g_filemanager.history_index],
                FM_MAX_PATH - 1);
        fm_load_directory(g_filemanager.current_path);
    }
}

void fm_go_forward(void) {
    if (g_filemanager.history_index < g_filemanager.history_count - 1) {
        g_filemanager.history_index++;
        strncpy(g_filemanager.current_path,
                g_filemanager.history[g_filemanager.history_index],
                FM_MAX_PATH - 1);
        fm_load_directory(g_filemanager.current_path);
    }
}

void fm_go_up(void) {
    // Üst dizine git
    char* last_slash = strrchr(g_filemanager.current_path, '/');
    if (last_slash && last_slash != g_filemanager.current_path) {
        *last_slash = '\0';
        fm_navigate(g_filemanager.current_path);
    } else if (strcmp(g_filemanager.current_path, "/") != 0) {
        fm_navigate("/");
    }
}

void fm_go_home(void) {
    fm_navigate("/");
}

void fm_refresh(void) {
    fm_load_directory(g_filemanager.current_path);
}

// ==================== DOSYA İŞLEMLERİ ====================

void fm_open_selected(void) {
    if (g_filemanager.selected_index < 0 || 
        g_filemanager.selected_index >= (int32_t)g_filemanager.file_count) {
        return;
    }
    
    FileEntry* entry = &g_filemanager.files[g_filemanager.selected_index];
    
    if (entry->is_directory) {
        // Klasöre gir
        char new_path[FM_MAX_PATH];
        if (strcmp(g_filemanager.current_path, "/") == 0) {
            snprintf(new_path, FM_MAX_PATH, "/%s", entry->name);
        } else {
            snprintf(new_path, FM_MAX_PATH, "%s/%s", 
                     g_filemanager.current_path, entry->name);
        }
        fm_navigate(new_path);
    } else {
        // Dosya türüne göre aç
        switch (entry->type) {
            case FILE_TYPE_TEXT:
            case FILE_TYPE_CODE:
                // Notepad ile aç
                // notepad_open(full_path);
                // app_launch_by_name("Notepad");
                break;
                
            case FILE_TYPE_IMAGE:
                // Resim görüntüleyici ile aç
                break;
                
            case FILE_TYPE_EXECUTABLE:
                // Çalıştır
                break;
                
            default:
                // Bilinmeyen dosya
                break;
        }
    }
}

void fm_create_folder(const char* name) {
    // VFS ile klasör oluştur
    // TODO: vfs_mkdir
    
    // Yenile
    fm_refresh();
}

void fm_create_file(const char* name) {
    // VFS ile dosya oluştur
    // TODO: vfs_create
    
    fm_refresh();
}

void fm_delete_selected(void) {
    // Seçili dosyaları sil
    for (uint32_t i = 0; i < g_filemanager.file_count; i++) {
        if (g_filemanager.files[i].selected) {
            // VFS ile sil
            // TODO: vfs_delete
        }
    }
    
    fm_refresh();
}

void fm_rename_start(void) {
    if (g_filemanager.selected_index < 0) return;
    
    g_filemanager.renaming = true;
    g_filemanager.rename_index = g_filemanager.selected_index;
    strcpy(g_filemanager.rename_buffer, 
           g_filemanager.files[g_filemanager.selected_index].name);
    g_filemanager.rename_cursor = strlen(g_filemanager.rename_buffer);
}

void fm_rename_end(void) {
    if (!g_filemanager.renaming) return;
    
    // VFS ile yeniden adlandır
    // TODO: vfs_rename
    
    g_filemanager.renaming = false;
    fm_refresh();
}

void fm_rename_cancel(void) {
    g_filemanager.renaming = false;
}

// ==================== PANO ====================

void fm_cut(void) {
    g_filemanager.clipboard_mode = CLIPBOARD_CUT;
    strcpy(g_filemanager.clipboard_path, g_filemanager.current_path);
    g_filemanager.clipboard_count = 0;
    
    for (uint32_t i = 0; i < g_filemanager.file_count && g_filemanager.clipboard_count < 64; i++) {
        if (g_filemanager.files[i].selected) {
            strcpy(g_filemanager.clipboard_files[g_filemanager.clipboard_count++],
                   g_filemanager.files[i].name);
        }
    }
}

void fm_copy(void) {
    g_filemanager.clipboard_mode = CLIPBOARD_COPY;
    strcpy(g_filemanager.clipboard_path, g_filemanager.current_path);
    g_filemanager.clipboard_count = 0;
    
    for (uint32_t i = 0; i < g_filemanager.file_count && g_filemanager.clipboard_count < 64; i++) {
        if (g_filemanager.files[i].selected) {
            strcpy(g_filemanager.clipboard_files[g_filemanager.clipboard_count++],
                   g_filemanager.files[i].name);
        }
    }
}

void fm_paste(void) {
    if (g_filemanager.clipboard_count == 0) return;
    
    for (uint32_t i = 0; i < g_filemanager.clipboard_count; i++) {
        // VFS ile kopyala/taşı
        // TODO: vfs_copy / vfs_move
    }
    
    if (g_filemanager.clipboard_mode == CLIPBOARD_CUT) {
        g_filemanager.clipboard_count = 0;
    }
    
    fm_refresh();
}

// ==================== SEÇİM ====================

void fm_select_all(void) {
    for (uint32_t i = 0; i < g_filemanager.file_count; i++) {
        g_filemanager.files[i].selected = true;
    }
    g_filemanager.selected_count = g_filemanager.file_count;
}

void fm_deselect_all(void) {
    for (uint32_t i = 0; i < g_filemanager.file_count; i++) {
        g_filemanager.files[i].selected = false;
    }
    g_filemanager.selected_count = 0;
}

// ==================== DİZİN YÜKLEME ====================

void fm_load_directory(const char* path) {
    g_filemanager.file_count = 0;
    fm_deselect_all();
    
    // VFS'den dizin listesi al
    // TODO: vfs_readdir
    
    // Şimdilik test verileri
    // Klasörler
    FileEntry* entry;
    
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "bin");
    entry->type = FILE_TYPE_FOLDER;
    entry->is_directory = true;
    entry->size = 0;
    
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "etc");
    entry->type = FILE_TYPE_FOLDER;
    entry->is_directory = true;
    entry->size = 0;
    
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "home");
    entry->type = FILE_TYPE_FOLDER;
    entry->is_directory = true;
    entry->size = 0;
    
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "tmp");
    entry->type = FILE_TYPE_FOLDER;
    entry->is_directory = true;
    entry->size = 0;
    
    // Dosyalar
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "README.txt");
    strcpy(entry->extension, "txt");
    entry->type = FILE_TYPE_TEXT;
    entry->is_directory = false;
    entry->size = 1024;
    
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "kernel.bin");
    strcpy(entry->extension, "bin");
    entry->type = FILE_TYPE_EXECUTABLE;
    entry->is_directory = false;
    entry->size = 524288;
    
    entry = &g_filemanager.files[g_filemanager.file_count++];
    strcpy(entry->name, "config.cfg");
    strcpy(entry->extension, "cfg");
    entry->type = FILE_TYPE_TEXT;
    entry->is_directory = false;
    entry->size = 256;
    
    // Sırala
    fm_sort_files();
    
    // İlk öğeyi seç
    if (g_filemanager.file_count > 0) {
        g_filemanager.selected_index = 0;
        g_filemanager.files[0].selected = true;
        g_filemanager.selected_count = 1;
    }
}

void fm_sort_files(void) {
    // Basit bubble sort (klasörler önce)
    for (uint32_t i = 0; i < g_filemanager.file_count - 1; i++) {
        for (uint32_t j = 0; j < g_filemanager.file_count - i - 1; j++) {
            bool swap = false;
            FileEntry* a = &g_filemanager.files[j];
            FileEntry* b = &g_filemanager.files[j + 1];
            
            // Klasörler her zaman önce
            if (!a->is_directory && b->is_directory) {
                swap = true;
            }
            else if (a->is_directory == b->is_directory) {
                // Aynı tipte ise sıralama kriterine göre
                switch (g_filemanager.sort_by) {
                    case SORT_BY_NAME:
                        if (strcmp(a->name, b->name) > 0) swap = true;
                        break;
                    case SORT_BY_SIZE:
                        if (a->size > b->size) swap = true;
                        break;
                    case SORT_BY_TYPE:
                        if (strcmp(a->extension, b->extension) > 0) swap = true;
                        break;
                    default:
                        break;
                }
                
                if (g_filemanager.sort_order == SORT_ORDER_DESC) {
                    swap = !swap;
                }
            }
            
            if (swap) {
                FileEntry temp = g_filemanager.files[j];
                g_filemanager.files[j] = g_filemanager.files[j + 1];
                g_filemanager.files[j + 1] = temp;
            }
        }
    }
}

FileType fm_get_file_type(const char* name) {
    const char* ext = strrchr(name, '.');
    if (!ext) return FILE_TYPE_UNKNOWN;
    ext++;
    
    // Metin dosyaları
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 ||
        strcmp(ext, "cfg") == 0 || strcmp(ext, "ini") == 0 ||
        strcmp(ext, "log") == 0) {
        return FILE_TYPE_TEXT;
    }
    
    // Kod dosyaları
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0 ||
        strcmp(ext, "cpp") == 0 || strcmp(ext, "py") == 0 ||
        strcmp(ext, "js") == 0 || strcmp(ext, "html") == 0 ||
        strcmp(ext, "css") == 0 || strcmp(ext, "asm") == 0) {
        return FILE_TYPE_CODE;
    }
    
    // Resim dosyaları
    if (strcmp(ext, "bmp") == 0 || strcmp(ext, "png") == 0 ||
        strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0 ||
        strcmp(ext, "gif") == 0) {
        return FILE_TYPE_IMAGE;
    }
    
    // Çalıştırılabilir
    if (strcmp(ext, "bin") == 0 || strcmp(ext, "elf") == 0 ||
        strcmp(ext, "exe") == 0) {
        return FILE_TYPE_EXECUTABLE;
    }
    
    // Arşiv
    if (strcmp(ext, "zip") == 0 || strcmp(ext, "tar") == 0 ||
        strcmp(ext, "gz") == 0) {
        return FILE_TYPE_ARCHIVE;
    }
    
    return FILE_TYPE_UNKNOWN;
}

const char* fm_get_file_icon(FileType type) {
    switch (type) {
        case FILE_TYPE_FOLDER:      return "📁";
        case FILE_TYPE_TEXT:        return "📄";
        case FILE_TYPE_CODE:        return "📝";
        case FILE_TYPE_IMAGE:       return "🖼";
        case FILE_TYPE_AUDIO:       return "🎵";
        case FILE_TYPE_VIDEO:       return "🎬";
        case FILE_TYPE_ARCHIVE:     return "📦";
        case FILE_TYPE_EXECUTABLE:  return "⚙";
        case FILE_TYPE_DOCUMENT:    return "📑";
        default:                    return "📄";
    }
}

const char* fm_format_size(uint64_t size) {
    static char buffer[32];
    
    if (size < 1024) {
        // sprintf(buffer, "%llu B", size);
        strcpy(buffer, "< 1 KB");
    } else if (size < 1024 * 1024) {
        // sprintf(buffer, "%.1f KB", size / 1024.0);
        strcpy(buffer, "KB");
    } else if (size < 1024 * 1024 * 1024) {
        // sprintf(buffer, "%.1f MB", size / (1024.0 * 1024.0));
        strcpy(buffer, "MB");
    } else {
        // sprintf(buffer, "%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
        strcpy(buffer, "GB");
    }
    
    return buffer;
}

void fm_add_to_history(const char* path) {
    // Aynı yol zaten son girişse ekleme
    if (g_filemanager.history_index >= 0 &&
        strcmp(g_filemanager.history[g_filemanager.history_index], path) == 0) {
        return;
    }
    
    // İleri geçmişi temizle
    g_filemanager.history_count = g_filemanager.history_index + 1;
    
    // Yer aç
    if (g_filemanager.history_count >= FM_MAX_HISTORY) {
        memmove(&g_filemanager.history[0], &g_filemanager.history[1],
                (FM_MAX_HISTORY - 1) * FM_MAX_PATH);
        g_filemanager.history_count--;
    }
    
    // Ekle
    strcpy(g_filemanager.history[g_filemanager.history_count], path);
    g_filemanager.history_index = g_filemanager.history_count;
    g_filemanager.history_count++;
}

// ==================== GÖRÜNÜM ====================

void fm_set_view_mode(ViewMode mode) {
    g_filemanager.view_mode = mode;
}

void fm_toggle_hidden(void) {
    g_filemanager.show_hidden = !g_filemanager.show_hidden;
    fm_refresh();
}

void fm_sort(SortBy by) {
    g_filemanager.sort_by = by;
    fm_sort_files();
}

// ==================== ARAMA ====================

void fm_search_start(void) {
    g_filemanager.search_active = true;
    g_filemanager.search_text[0] = '\0';
}

void fm_search_end(void) {
    g_filemanager.search_active = false;
    fm_refresh();
}

void fm_search_execute(void) {
    // Arama sonuçlarını filtrele
    // TODO: Arama implementasyonu
}

// ==================== UI ÇİZİM ====================

void fm_draw_toolbar(void) {
    int x = g_filemanager.window_x;
    int y = g_filemanager.window_y;
    int w = g_filemanager.window_width;
    int h = g_filemanager.toolbar_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_TOOLBAR_BG);
    
    // Butonlar
    // Geri, İleri, Yukarı, Yenile, Ana dizin
    // draw_button(...);
}

void fm_draw_path_bar(void) {
    int x = g_filemanager.window_x;
    int y = g_filemanager.window_y + g_filemanager.toolbar_height;
    int w = g_filemanager.window_width;
    int h = g_filemanager.path_bar_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_PATH_BG);
    
    // Yol
    // draw_text(x + 10, y + 8, g_filemanager.current_path, COLOR_TEXT);
}

void fm_draw_sidebar(void) {
    int x = g_filemanager.window_x;
    int y = g_filemanager.window_y + g_filemanager.toolbar_height + g_filemanager.path_bar_height;
    int w = g_filemanager.sidebar_width;
    int h = g_filemanager.window_height - g_filemanager.toolbar_height - 
            g_filemanager.path_bar_height - g_filemanager.status_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_SIDEBAR_BG);
    
    // Başlık
    // draw_text(x + 10, y + 8, "Hizli Erisim", COLOR_TEXT);
    
    // Öğeler
    int item_y = y + 30;
    int item_h = 35;
    
    for (int i = 0; quick_access[i].name != NULL; i++) {
        // draw_text(x + 35, item_y + 10, quick_access[i].name, COLOR_TEXT);
        item_y += item_h;
    }
}

void fm_draw_content(void) {
    int x = g_filemanager.window_x + 
            (g_filemanager.show_sidebar ? g_filemanager.sidebar_width : 0);
    int y = g_filemanager.window_y + g_filemanager.toolbar_height + g_filemanager.path_bar_height;
    int w = g_filemanager.window_width - 
            (g_filemanager.show_sidebar ? g_filemanager.sidebar_width : 0);
    int h = g_filemanager.window_height - g_filemanager.toolbar_height - 
            g_filemanager.path_bar_height - g_filemanager.status_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_CONTENT_BG);
    
    // Liste görünümü
    int item_h = 30;
    int item_y = y;
    
    for (uint32_t i = g_filemanager.scroll_offset; 
         i < g_filemanager.file_count && item_y < y + h; i++) {
        
        fm_draw_file_entry(&g_filemanager.files[i], x, item_y, 
                          g_filemanager.files[i].selected);
        item_y += item_h;
    }
    
    // Boş dizin mesajı
    if (g_filemanager.file_count == 0) {
        // draw_text_centered(x + w/2, y + h/2, "Bu klasor bos", COLOR_TEXT_DIM);
    }
}

void fm_draw_file_entry(FileEntry* entry, int32_t x, int32_t y, bool selected) {
    int w = g_filemanager.window_width - 
            (g_filemanager.show_sidebar ? g_filemanager.sidebar_width : 0);
    int h = 30;
    
    // Arka plan
    uint32_t bg = selected ? COLOR_ITEM_SELECTED : COLOR_CONTENT_BG;
    // draw_filled_rect(x, y, w, h, bg);
    
    // İkon
    const char* icon = fm_get_file_icon(entry->type);
    // draw_text(x + 10, y + 7, icon, entry->is_directory ? COLOR_FOLDER : COLOR_FILE);
    
    // İsim
    uint32_t name_color = entry->is_directory ? COLOR_FOLDER : COLOR_TEXT;
    // draw_text(x + 35, y + 8, entry->name, name_color);
    
    // Boyut (dosyalar için)
    if (!entry->is_directory) {
        // draw_text(x + w - 100, y + 8, fm_format_size(entry->size), COLOR_TEXT_DIM);
    }
}

void fm_draw_status_bar(void) {
    int x = g_filemanager.window_x;
    int y = g_filemanager.window_y + g_filemanager.window_height - g_filemanager.status_height;
    int w = g_filemanager.window_width;
    int h = g_filemanager.status_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_STATUS_BG);
    
    // Öğe sayısı
    char status[64];
    // sprintf(status, "%u oge", g_filemanager.file_count);
    // draw_text(x + 10, y + 5, status, 0xFFFFFF);
    
    // Seçili sayısı
    if (g_filemanager.selected_count > 1) {
        // sprintf(status, "%u secili", g_filemanager.selected_count);
        // draw_text(x + 150, y + 5, status, 0xFFFFFF);
    }
}

void fm_draw_context_menu(void) {
    int x = g_filemanager.window_x + g_filemanager.context_menu_x;
    int y = g_filemanager.window_y + g_filemanager.context_menu_y;
    int w = 150;
    int item_h = 25;
    
    const char* items[] = {
        "Ac",
        "Kes",
        "Kopyala",
        "Yapistir",
        "---",
        "Yeniden Adlandir",
        "Sil",
        "---",
        "Ozellikler",
        NULL
    };
    
    int item_count = 0;
    while (items[item_count]) item_count++;
    
    int h = item_count * item_h + 10;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_ITEM_BG);
    // draw_rect(x, y, w, h, COLOR_TEXT_DIM);
    
    // Öğeler
    int item_y = y + 5;
    for (int i = 0; items[i]; i++) {
        if (strcmp(items[i], "---") == 0) {
            // Ayırıcı
            // draw_line(x + 5, item_y + item_h/2, x + w - 5, item_y + item_h/2, COLOR_TEXT_DIM);
        } else {
            // draw_text(x + 15, item_y + 5, items[i], COLOR_TEXT);
        }
        item_y += item_h;
    }
}
