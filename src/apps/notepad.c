#include "notepad.h"
#include "../kernel/kernel.h"
#include "../vfs/vfs.h"
#include <string.h>

/*
 * ============================================
 *           MyOS Notepad
 * ============================================
 */

// ==================== GLOBAL ====================

NotepadState g_notepad;

// ==================== RENKLER ====================

#define COLOR_NOTEPAD_BG        0x1E1E1E
#define COLOR_MENU_BG           0x2D2D2D
#define COLOR_TOOLBAR_BG        0x2D2D2D
#define COLOR_CONTENT_BG        0x1E1E1E
#define COLOR_TEXT              0xD4D4D4
#define COLOR_TEXT_SELECTED     0xFFFFFF
#define COLOR_SELECTION_BG      0x264F78
#define COLOR_CURSOR            0xAEAFAD
#define COLOR_LINE_NUMBER       0x858585
#define COLOR_LINE_NUMBER_BG    0x1E1E1E
#define COLOR_CURRENT_LINE      0x282828
#define COLOR_STATUS_BG         0x007ACC
#define COLOR_STATUS_TEXT       0xFFFFFF
#define COLOR_SEARCH_BG         0x3C3C3C
#define COLOR_SEARCH_MATCH      0x613214

// ==================== YAŞAM DÖNGÜSÜ ====================

void notepad_init(void) {
    memset(&g_notepad, 0, sizeof(NotepadState));
    
    // Pencere boyutu
    g_notepad.window_width = 700;
    g_notepad.window_height = 500;
    g_notepad.window_x = 100;
    g_notepad.window_y = 80;
    
    // UI boyutları
    g_notepad.menu_height = 0;          // Menü yok şimdilik
    g_notepad.toolbar_height = 35;
    g_notepad.status_height = 25;
    
    g_notepad.content_x = 0;
    g_notepad.content_y = g_notepad.toolbar_height;
    g_notepad.content_width = g_notepad.window_width;
    g_notepad.content_height = g_notepad.window_height - g_notepad.toolbar_height - g_notepad.status_height;
    
    // Görünüm ayarları
    g_notepad.show_line_numbers = true;
    g_notepad.line_number_width = 50;
    g_notepad.show_status_bar = true;
    g_notepad.word_wrap = false;
    
    // Font boyutuna göre görünen satır/sütun
    int char_width = 8;
    int char_height = 16;
    g_notepad.visible_lines = g_notepad.content_height / char_height;
    g_notepad.visible_cols = (g_notepad.content_width - g_notepad.line_number_width) / char_width;
    
    // Cursor
    g_notepad.cursor_line = 0;
    g_notepad.cursor_col = 0;
    g_notepad.cursor_visible = true;
    g_notepad.cursor_blink = 0;
    
    // Undo
    g_notepad.undo_count = 0;
    g_notepad.undo_index = 0;
    
    // Arama
    g_notepad.search_active = false;
    g_notepad.search_case_sensitive = false;
    
    // Yeni belge
    notepad_new();
}

void notepad_update(void) {
    // Cursor yanıp sönme
    g_notepad.cursor_blink++;
    if (g_notepad.cursor_blink >= 30) {
        g_notepad.cursor_visible = !g_notepad.cursor_visible;
        g_notepad.cursor_blink = 0;
    }
}

void notepad_render(void) {
    // Arka plan
    // draw_filled_rect(g_notepad.window_x, g_notepad.window_y,
    //                  g_notepad.window_width, g_notepad.window_height,
    //                  COLOR_NOTEPAD_BG);
    
    // Toolbar
    notepad_draw_toolbar();
    
    // İçerik alanı
    notepad_draw_content();
    
    // Satır numaraları
    if (g_notepad.show_line_numbers) {
        notepad_draw_line_numbers();
    }
    
    // Seçim
    if (notepad_has_selection()) {
        notepad_draw_selection();
    }
    
    // Cursor
    notepad_draw_cursor();
    
    // Durum çubuğu
    if (g_notepad.show_status_bar) {
        notepad_draw_status_bar();
    }
    
    // Arama çubuğu
    if (g_notepad.search_active) {
        notepad_draw_search_bar();
    }
}

void notepad_handle_input(uint8_t key) {
    // Arama modunda
    if (g_notepad.search_active) {
        switch (key) {
            case 27:    // Escape
                g_notepad.search_active = false;
                return;
            case '\n':
            case '\r':
                notepad_find_next();
                return;
            case '\b':
                if (g_notepad.search_cursor > 0) {
                    g_notepad.search_cursor--;
                    memmove(&g_notepad.search_text[g_notepad.search_cursor],
                            &g_notepad.search_text[g_notepad.search_cursor + 1],
                            strlen(&g_notepad.search_text[g_notepad.search_cursor + 1]) + 1);
                }
                return;
            default:
                if (key >= 32 && key < 127 && g_notepad.search_cursor < 255) {
                    g_notepad.search_text[g_notepad.search_cursor++] = key;
                    g_notepad.search_text[g_notepad.search_cursor] = '\0';
                }
                return;
        }
    }
    
    // Normal mod
    switch (key) {
        // Karakter girişi
        case '\n':
        case '\r':
            notepad_insert_newline();
            break;
            
        case '\b':
            notepad_backspace();
            break;
            
        case '\t':
            // Tab -> boşluklar
            for (int i = 0; i < NOTEPAD_TAB_SIZE; i++) {
                notepad_insert_char(' ');
            }
            break;
            
        // Ctrl kombinasyonları (ASCII kontrol karakterleri)
        case 1:     // Ctrl+A - Select All
            notepad_select_all();
            break;
        case 3:     // Ctrl+C - Copy
            notepad_copy();
            break;
        case 6:     // Ctrl+F - Find
            notepad_find();
            break;
        case 7:     // Ctrl+G - Goto Line
            // TODO: Goto line dialog
            break;
        case 8:     // Ctrl+H - Replace (aynı zamanda Backspace olabilir)
            // notepad_replace();
            break;
        case 14:    // Ctrl+N - New
            notepad_new();
            break;
        case 15:    // Ctrl+O - Open
            // TODO: File dialog
            break;
        case 19:    // Ctrl+S - Save
            notepad_save();
            break;
        case 22:    // Ctrl+V - Paste
            notepad_paste();
            break;
        case 24:    // Ctrl+X - Cut
            notepad_cut();
            break;
        case 25:    // Ctrl+Y - Redo
            notepad_redo();
            break;
        case 26:    // Ctrl+Z - Undo
            notepad_undo();
            break;
            
        case 27:    // Escape
            if (notepad_has_selection()) {
                notepad_clear_selection();
            } else {
                app_close(g_notepad.app_id);
            }
            break;
            
        case 127:   // Delete
            notepad_delete_char();
            break;
            
        // Normal karakterler
        default:
            if (key >= 32 && key < 127) {
                notepad_insert_char(key);
            }
            break;
    }
    
    // Değişiklik işareti
    if (key != 27 && key != 3) {  // Escape ve Copy hariç
        // Belge değişti
    }
}

void notepad_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    int32_t local_x = x - g_notepad.window_x;
    int32_t local_y = y - g_notepad.window_y;
    
    // Toolbar tıklama
    if (local_y < g_notepad.toolbar_height) {
        if (buttons & 1) {
            // Buton tıklamaları
            int btn_x = 10;
            int btn_w = 30;
            int btn_gap = 5;
            
            // Yeni
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_new();
                return;
            }
            btn_x += btn_w + btn_gap;
            
            // Aç
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                // notepad_open_dialog();
                return;
            }
            btn_x += btn_w + btn_gap;
            
            // Kaydet
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_save();
                return;
            }
            btn_x += btn_w + btn_gap + 10;  // Ayırıcı
            
            // Kes
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_cut();
                return;
            }
            btn_x += btn_w + btn_gap;
            
            // Kopyala
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_copy();
                return;
            }
            btn_x += btn_w + btn_gap;
            
            // Yapıştır
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_paste();
                return;
            }
            btn_x += btn_w + btn_gap + 10;
            
            // Geri al
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_undo();
                return;
            }
            btn_x += btn_w + btn_gap;
            
            // Yinele
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_redo();
                return;
            }
            btn_x += btn_w + btn_gap + 10;
            
            // Bul
            if (local_x >= btn_x && local_x < btn_x + btn_w) {
                notepad_find();
                return;
            }
        }
        return;
    }
    
    // İçerik alanı tıklama
    if (local_y >= g_notepad.content_y && 
        local_y < g_notepad.window_height - g_notepad.status_height) {
        
        int content_x = local_x - g_notepad.line_number_width;
        int content_y = local_y - g_notepad.content_y;
        
        if (content_x < 0) return;  // Satır numaraları üzerinde
        
        int char_width = 8;
        int char_height = 16;
        
        uint32_t clicked_line = g_notepad.scroll_y + (content_y / char_height);
        uint32_t clicked_col = g_notepad.scroll_x + (content_x / char_width);
        
        // Sınırları kontrol et
        if (clicked_line >= g_notepad.document.line_count) {
            clicked_line = g_notepad.document.line_count > 0 ? g_notepad.document.line_count - 1 : 0;
        }
        
        NoteLine* line = &g_notepad.document.lines[clicked_line];
        if (clicked_col > line->length) {
            clicked_col = line->length;
        }
        
        if (buttons & 1) {
            // Sol tık - cursor taşı
            static bool was_pressed = false;
            
            if (!was_pressed) {
                // Yeni tıklama
                notepad_clear_selection();
                g_notepad.cursor_line = clicked_line;
                g_notepad.cursor_col = clicked_col;
                notepad_start_selection();
            } else {
                // Sürükleme - seçim güncelle
                g_notepad.cursor_line = clicked_line;
                g_notepad.cursor_col = clicked_col;
                notepad_update_selection();
            }
            was_pressed = true;
        } else {
            // was_pressed = false;
        }
    }
}

void notepad_cleanup(void) {
    // Kaydedilmemiş değişiklik kontrolü
    if (g_notepad.document.modified) {
        // TODO: "Kaydet?" dialog
    }
}

// ==================== KAYIT ====================

uint32_t notepad_register(void) {
    uint32_t id = app_register("Notepad", "1.0", APP_TYPE_UTILITY);
    
    Application* app = app_get(id);
    if (app) {
        app->init = notepad_init;
        app->update = notepad_update;
        app->render = notepad_render;
        app->handle_input = notepad_handle_input;
        app->handle_mouse = notepad_handle_mouse;
        app->cleanup = notepad_cleanup;
        
        app->width = 700;
        app->height = 500;
    }
    
    g_notepad.app_id = id;
    return id;
}

// ==================== DOSYA İŞLEMLERİ ====================

void notepad_new(void) {
    memset(&g_notepad.document, 0, sizeof(NoteDocument));
    
    g_notepad.document.line_count = 1;
    g_notepad.document.lines[0].length = 0;
    g_notepad.document.lines[0].text[0] = '\0';
    
    strcpy(g_notepad.document.filename, "Adsiz");
    g_notepad.document.filepath[0] = '\0';
    g_notepad.document.modified = false;
    g_notepad.document.new_file = true;
    g_notepad.document.utf8 = true;
    
    g_notepad.cursor_line = 0;
    g_notepad.cursor_col = 0;
    g_notepad.scroll_x = 0;
    g_notepad.scroll_y = 0;
    
    notepad_clear_selection();
    
    g_notepad.undo_count = 0;
    g_notepad.undo_index = 0;
    
    notepad_update_title();
}

void notepad_open(const char* filepath) {
    // VFS'den dosya oku
    // TODO: vfs_read implementasyonu
    
    /*
    FILE* f = vfs_open(filepath, "r");
    if (!f) {
        // Hata
        return;
    }
    
    notepad_new();
    
    char line_buffer[NOTEPAD_MAX_LINE_LEN];
    while (vfs_gets(line_buffer, NOTEPAD_MAX_LINE_LEN, f)) {
        // Satır sonu karakterlerini kaldır
        int len = strlen(line_buffer);
        while (len > 0 && (line_buffer[len-1] == '\n' || line_buffer[len-1] == '\r')) {
            line_buffer[--len] = '\0';
        }
        
        if (g_notepad.document.line_count > 0 || line_buffer[0] != '\0') {
            NoteLine* line = &g_notepad.document.lines[g_notepad.document.line_count];
            strcpy(line->text, line_buffer);
            line->length = len;
            g_notepad.document.line_count++;
        }
    }
    
    vfs_close(f);
    */
    
    // Şimdilik test
    notepad_new();
    strcpy(g_notepad.document.lines[0].text, "Merhaba MyOS!");
    g_notepad.document.lines[0].length = 13;
    
    strncpy(g_notepad.document.filepath, filepath, 511);
    
    // Dosya adını çıkar
    const char* name = filepath;
    const char* p = filepath;
    while (*p) {
        if (*p == '/') name = p + 1;
        p++;
    }
    strncpy(g_notepad.document.filename, name, 255);
    
    g_notepad.document.new_file = false;
    g_notepad.document.modified = false;
    
    notepad_update_title();
}

void notepad_save(void) {
    if (g_notepad.document.new_file) {
        // Dosya adı yok, save_as gerekli
        // TODO: Save as dialog
        notepad_save_as("/home/document.txt");
        return;
    }
    
    notepad_save_as(g_notepad.document.filepath);
}

void notepad_save_as(const char* filepath) {
    // VFS'e dosya yaz
    // TODO: vfs_write implementasyonu
    
    /*
    FILE* f = vfs_open(filepath, "w");
    if (!f) {
        // Hata
        return;
    }
    
    for (uint32_t i = 0; i < g_notepad.document.line_count; i++) {
        vfs_puts(g_notepad.document.lines[i].text, f);
        vfs_putc('\n', f);
    }
    
    vfs_close(f);
    */
    
    strncpy(g_notepad.document.filepath, filepath, 511);
    
    // Dosya adını çıkar
    const char* name = filepath;
    const char* p = filepath;
    while (*p) {
        if (*p == '/') name = p + 1;
        p++;
    }
    strncpy(g_notepad.document.filename, name, 255);
    
    g_notepad.document.new_file = false;
    g_notepad.document.modified = false;
    
    notepad_update_title();
}

bool notepad_close(void) {
    if (g_notepad.document.modified) {
        // TODO: Kaydet dialog
        return false;
    }
    return true;
}

// ==================== METİN DÜZENLEME ====================

void notepad_insert_char(char c) {
    NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
    
    if (line->length >= NOTEPAD_MAX_LINE_LEN - 1) return;
    
    // Seçim varsa sil
    if (notepad_has_selection()) {
        notepad_delete_selection();
        line = &g_notepad.document.lines[g_notepad.cursor_line];
    }
    
    // Undo kaydet
    char undo_data[2] = {c, '\0'};
    notepad_push_undo(UNDO_INSERT_CHAR, g_notepad.cursor_line, g_notepad.cursor_col, undo_data);
    
    // Karakteri ekle
    memmove(&line->text[g_notepad.cursor_col + 1],
            &line->text[g_notepad.cursor_col],
            line->length - g_notepad.cursor_col + 1);
    
    line->text[g_notepad.cursor_col] = c;
    line->length++;
    g_notepad.cursor_col++;
    
    g_notepad.document.modified = true;
    notepad_scroll_to_cursor();
}

void notepad_insert_text(const char* text) {
    while (*text) {
        if (*text == '\n') {
            notepad_insert_newline();
        } else {
            notepad_insert_char(*text);
        }
        text++;
    }
}

void notepad_insert_newline(void) {
    if (g_notepad.document.line_count >= NOTEPAD_MAX_LINES - 1) return;
    
    // Seçim varsa sil
    if (notepad_has_selection()) {
        notepad_delete_selection();
    }
    
    NoteLine* current = &g_notepad.document.lines[g_notepad.cursor_line];
    
    // Undo kaydet
    notepad_push_undo(UNDO_INSERT_LINE, g_notepad.cursor_line, g_notepad.cursor_col, "");
    
    // Satırları aşağı kaydır
    memmove(&g_notepad.document.lines[g_notepad.cursor_line + 2],
            &g_notepad.document.lines[g_notepad.cursor_line + 1],
            (g_notepad.document.line_count - g_notepad.cursor_line - 1) * sizeof(NoteLine));
    
    // Yeni satır
    NoteLine* next = &g_notepad.document.lines[g_notepad.cursor_line + 1];
    
    // Cursor'dan sonraki kısmı yeni satıra taşı
    strcpy(next->text, &current->text[g_notepad.cursor_col]);
    next->length = current->length - g_notepad.cursor_col;
    
    // Mevcut satırı kes
    current->text[g_notepad.cursor_col] = '\0';
    current->length = g_notepad.cursor_col;
    
    g_notepad.document.line_count++;
    g_notepad.cursor_line++;
    g_notepad.cursor_col = 0;
    
    g_notepad.document.modified = true;
    notepad_scroll_to_cursor();
}

void notepad_delete_char(void) {
    NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
    
    // Seçim varsa seçimi sil
    if (notepad_has_selection()) {
        notepad_delete_selection();
        return;
    }
    
    if (g_notepad.cursor_col < line->length) {
        // Satır içinde sil
        char deleted = line->text[g_notepad.cursor_col];
        char undo_data[2] = {deleted, '\0'};
        notepad_push_undo(UNDO_DELETE_CHAR, g_notepad.cursor_line, g_notepad.cursor_col, undo_data);
        
        memmove(&line->text[g_notepad.cursor_col],
                &line->text[g_notepad.cursor_col + 1],
                line->length - g_notepad.cursor_col);
        line->length--;
        
        g_notepad.document.modified = true;
    }
    else if (g_notepad.cursor_line < g_notepad.document.line_count - 1) {
        // Sonraki satırı bu satıra birleştir
        NoteLine* next = &g_notepad.document.lines[g_notepad.cursor_line + 1];
        
        if (line->length + next->length < NOTEPAD_MAX_LINE_LEN) {
            strcat(line->text, next->text);
            line->length += next->length;
            
            // Satırları yukarı kaydır
            memmove(&g_notepad.document.lines[g_notepad.cursor_line + 1],
                    &g_notepad.document.lines[g_notepad.cursor_line + 2],
                    (g_notepad.document.line_count - g_notepad.cursor_line - 2) * sizeof(NoteLine));
            
            g_notepad.document.line_count--;
            g_notepad.document.modified = true;
        }
    }
}

void notepad_backspace(void) {
    // Seçim varsa seçimi sil
    if (notepad_has_selection()) {
        notepad_delete_selection();
        return;
    }
    
    if (g_notepad.cursor_col > 0) {
        g_notepad.cursor_col--;
        notepad_delete_char();
    }
    else if (g_notepad.cursor_line > 0) {
        // Önceki satırın sonuna git ve birleştir
        NoteLine* prev = &g_notepad.document.lines[g_notepad.cursor_line - 1];
        g_notepad.cursor_line--;
        g_notepad.cursor_col = prev->length;
        notepad_delete_char();
    }
    
    notepad_scroll_to_cursor();
}

void notepad_delete_line(void) {
    if (g_notepad.document.line_count <= 1) {
        // Son satır - sadece temizle
        g_notepad.document.lines[0].text[0] = '\0';
        g_notepad.document.lines[0].length = 0;
        g_notepad.cursor_col = 0;
        return;
    }
    
    // Undo kaydet
    notepad_push_undo(UNDO_DELETE_LINE, g_notepad.cursor_line, 0, 
                      g_notepad.document.lines[g_notepad.cursor_line].text);
    
    // Satırları yukarı kaydır
    memmove(&g_notepad.document.lines[g_notepad.cursor_line],
            &g_notepad.document.lines[g_notepad.cursor_line + 1],
            (g_notepad.document.line_count - g_notepad.cursor_line - 1) * sizeof(NoteLine));
    
    g_notepad.document.line_count--;
    
    if (g_notepad.cursor_line >= g_notepad.document.line_count) {
        g_notepad.cursor_line = g_notepad.document.line_count - 1;
    }
    
    NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
    if (g_notepad.cursor_col > line->length) {
        g_notepad.cursor_col = line->length;
    }
    
    g_notepad.document.modified = true;
}

void notepad_delete_selection(void) {
    if (!notepad_has_selection()) return;
    
    NoteSelection* sel = &g_notepad.selection;
    
    // Seçimi normalize et (start < end)
    uint32_t start_line = sel->start_line;
    uint32_t start_col = sel->start_col;
    uint32_t end_line = sel->end_line;
    uint32_t end_col = sel->end_col;
    
    if (start_line > end_line || (start_line == end_line && start_col > end_col)) {
        uint32_t tmp = start_line; start_line = end_line; end_line = tmp;
        tmp = start_col; start_col = end_col; end_col = tmp;
    }
    
    if (start_line == end_line) {
        // Tek satır seçimi
        NoteLine* line = &g_notepad.document.lines[start_line];
        memmove(&line->text[start_col],
                &line->text[end_col],
                line->length - end_col + 1);
        line->length -= (end_col - start_col);
    } else {
        // Çok satır seçimi
        NoteLine* first = &g_notepad.document.lines[start_line];
        NoteLine* last = &g_notepad.document.lines[end_line];
        
        // İlk satırın başı + son satırın sonu
        first->text[start_col] = '\0';
        strcat(first->text, &last->text[end_col]);
        first->length = start_col + (last->length - end_col);
        
        // Aradaki satırları sil
        uint32_t lines_to_remove = end_line - start_line;
        memmove(&g_notepad.document.lines[start_line + 1],
                &g_notepad.document.lines[end_line + 1],
                (g_notepad.document.line_count - end_line - 1) * sizeof(NoteLine));
        g_notepad.document.line_count -= lines_to_remove;
    }
    
    g_notepad.cursor_line = start_line;
    g_notepad.cursor_col = start_col;
    
    notepad_clear_selection();
    g_notepad.document.modified = true;
}

// ==================== CURSOR HAREKETİ ====================

void notepad_cursor_left(void) {
    if (g_notepad.cursor_col > 0) {
        g_notepad.cursor_col--;
    } else if (g_notepad.cursor_line > 0) {
        g_notepad.cursor_line--;
        g_notepad.cursor_col = g_notepad.document.lines[g_notepad.cursor_line].length;
    }
    g_notepad.desired_col = g_notepad.cursor_col;
    notepad_scroll_to_cursor();
}

void notepad_cursor_right(void) {
    NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
    
    if (g_notepad.cursor_col < line->length) {
        g_notepad.cursor_col++;
    } else if (g_notepad.cursor_line < g_notepad.document.line_count - 1) {
        g_notepad.cursor_line++;
        g_notepad.cursor_col = 0;
    }
    g_notepad.desired_col = g_notepad.cursor_col;
    notepad_scroll_to_cursor();
}

void notepad_cursor_up(void) {
    if (g_notepad.cursor_line > 0) {
        g_notepad.cursor_line--;
        NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
        g_notepad.cursor_col = g_notepad.desired_col;
        if (g_notepad.cursor_col > line->length) {
            g_notepad.cursor_col = line->length;
        }
    }
    notepad_scroll_to_cursor();
}

void notepad_cursor_down(void) {
    if (g_notepad.cursor_line < g_notepad.document.line_count - 1) {
        g_notepad.cursor_line++;
        NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
        g_notepad.cursor_col = g_notepad.desired_col;
        if (g_notepad.cursor_col > line->length) {
            g_notepad.cursor_col = line->length;
        }
    }
    notepad_scroll_to_cursor();
}

void notepad_cursor_home(void) {
    g_notepad.cursor_col = 0;
    g_notepad.desired_col = 0;
    notepad_scroll_to_cursor();
}

void notepad_cursor_end(void) {
    g_notepad.cursor_col = g_notepad.document.lines[g_notepad.cursor_line].length;
    g_notepad.desired_col = g_notepad.cursor_col;
    notepad_scroll_to_cursor();
}

void notepad_cursor_page_up(void) {
    if (g_notepad.cursor_line >= g_notepad.visible_lines) {
        g_notepad.cursor_line -= g_notepad.visible_lines;
    } else {
        g_notepad.cursor_line = 0;
    }
    
    NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
    if (g_notepad.cursor_col > line->length) {
        g_notepad.cursor_col = line->length;
    }
    notepad_scroll_to_cursor();
}

void notepad_cursor_page_down(void) {
    g_notepad.cursor_line += g_notepad.visible_lines;
    if (g_notepad.cursor_line >= g_notepad.document.line_count) {
        g_notepad.cursor_line = g_notepad.document.line_count - 1;
    }
    
    NoteLine* line = &g_notepad.document.lines[g_notepad.cursor_line];
    if (g_notepad.cursor_col > line->length) {
        g_notepad.cursor_col = line->length;
    }
    notepad_scroll_to_cursor();
}

void notepad_cursor_doc_start(void) {
    g_notepad.cursor_line = 0;
    g_notepad.cursor_col = 0;
    g_notepad.desired_col = 0;
    notepad_scroll_to_cursor();
}

void notepad_cursor_doc_end(void) {
    g_notepad.cursor_line = g_notepad.document.line_count - 1;
    g_notepad.cursor_col = g_notepad.document.lines[g_notepad.cursor_line].length;
    g_notepad.desired_col = g_notepad.cursor_col;
    notepad_scroll_to_cursor();
}

void notepad_goto_line(uint32_t line) {
    if (line >= g_notepad.document.line_count) {
        line = g_notepad.document.line_count - 1;
    }
    g_notepad.cursor_line = line;
    g_notepad.cursor_col = 0;
    notepad_scroll_to_cursor();
}

// ==================== SEÇİM ====================

void notepad_select_all(void) {
    g_notepad.selection.active = true;
    g_notepad.selection.start_line = 0;
    g_notepad.selection.start_col = 0;
    g_notepad.selection.end_line = g_notepad.document.line_count - 1;
    g_notepad.selection.end_col = g_notepad.document.lines[g_notepad.selection.end_line].length;
    
    g_notepad.cursor_line = g_notepad.selection.end_line;
    g_notepad.cursor_col = g_notepad.selection.end_col;
}

void notepad_start_selection(void) {
    g_notepad.selection.active = true;
    g_notepad.selection.start_line = g_notepad.cursor_line;
    g_notepad.selection.start_col = g_notepad.cursor_col;
    g_notepad.selection.end_line = g_notepad.cursor_line;
    g_notepad.selection.end_col = g_notepad.cursor_col;
}

void notepad_update_selection(void) {
    g_notepad.selection.end_line = g_notepad.cursor_line;
    g_notepad.selection.end_col = g_notepad.cursor_col;
}

void notepad_clear_selection(void) {
    g_notepad.selection.active = false;
}

bool notepad_has_selection(void) {
    if (!g_notepad.selection.active) return false;
    
    return !(g_notepad.selection.start_line == g_notepad.selection.end_line &&
             g_notepad.selection.start_col == g_notepad.selection.end_col);
}

// ==================== PANO ====================

// Basit dahili pano
static char clipboard[65536];
static uint32_t clipboard_length = 0;

void notepad_cut(void) {
    notepad_copy();
    notepad_delete_selection();
}

void notepad_copy(void) {
    if (!notepad_has_selection()) return;
    
    NoteSelection* sel = &g_notepad.selection;
    
    // Normalize
    uint32_t start_line = sel->start_line;
    uint32_t start_col = sel->start_col;
    uint32_t end_line = sel->end_line;
    uint32_t end_col = sel->end_col;
    
    if (start_line > end_line || (start_line == end_line && start_col > end_col)) {
        uint32_t tmp = start_line; start_line = end_line; end_line = tmp;
        tmp = start_col; start_col = end_col; end_col = tmp;
    }
    
    clipboard_length = 0;
    
    for (uint32_t line = start_line; line <= end_line; line++) {
        NoteLine* l = &g_notepad.document.lines[line];
        
        uint32_t col_start = (line == start_line) ? start_col : 0;
        uint32_t col_end = (line == end_line) ? end_col : l->length;
        
        for (uint32_t col = col_start; col < col_end && clipboard_length < 65535; col++) {
            clipboard[clipboard_length++] = l->text[col];
        }
        
        if (line < end_line && clipboard_length < 65535) {
            clipboard[clipboard_length++] = '\n';
        }
    }
    clipboard[clipboard_length] = '\0';
}

void notepad_paste(void) {
    if (clipboard_length == 0) return;
    
    if (notepad_has_selection()) {
        notepad_delete_selection();
    }
    
    notepad_insert_text(clipboard);
}

// ==================== UNDO/REDO ====================

void notepad_push_undo(UndoType type, uint32_t line, uint32_t col, const char* data) {
    if (g_notepad.undo_count >= NOTEPAD_MAX_UNDO) {
        // En eski kaydı sil
        memmove(&g_notepad.undo_stack[0], &g_notepad.undo_stack[1],
                (NOTEPAD_MAX_UNDO - 1) * sizeof(UndoRecord));
        g_notepad.undo_count--;
    }
    
    UndoRecord* record = &g_notepad.undo_stack[g_notepad.undo_count];
    record->type = type;
    record->line = line;
    record->column = col;
    strncpy(record->data, data, NOTEPAD_MAX_LINE_LEN - 1);
    record->data_length = strlen(data);
    
    g_notepad.undo_count++;
    g_notepad.undo_index = g_notepad.undo_count;
}

void notepad_undo(void) {
    if (g_notepad.undo_index == 0) return;
    
    g_notepad.undo_index--;
    UndoRecord* record = &g_notepad.undo_stack[g_notepad.undo_index];
    
    // Geri al
    g_notepad.cursor_line = record->line;
    g_notepad.cursor_col = record->column;
    
    switch (record->type) {
        case UNDO_INSERT_CHAR:
            // Eklenen karakteri sil
            notepad_delete_char();
            break;
            
        case UNDO_DELETE_CHAR:
            // Silinen karakteri geri ekle
            notepad_insert_char(record->data[0]);
            g_notepad.cursor_col--;  // Cursor'u geri al
            break;
            
        case UNDO_INSERT_LINE:
            // Eklenen satırı sil
            notepad_delete_line();
            break;
            
        case UNDO_DELETE_LINE:
            // Silinen satırı geri ekle
            // TODO
            break;
            
        default:
            break;
    }
    
    notepad_scroll_to_cursor();
}

void notepad_redo(void) {
    if (g_notepad.undo_index >= g_notepad.undo_count) return;
    
    UndoRecord* record = &g_notepad.undo_stack[g_notepad.undo_index];
    g_notepad.undo_index++;
    
    g_notepad.cursor_line = record->line;
    g_notepad.cursor_col = record->column;
    
    switch (record->type) {
        case UNDO_INSERT_CHAR:
            notepad_insert_char(record->data[0]);
            break;
            
        case UNDO_DELETE_CHAR:
            notepad_delete_char();
            break;
            
        default:
            break;
    }
    
    notepad_scroll_to_cursor();
}

// ==================== ARAMA ====================

void notepad_find(void) {
    g_notepad.search_active = true;
    g_notepad.search_text[0] = '\0';
    g_notepad.search_cursor = 0;
}

void notepad_find_next(void) {
    if (g_notepad.search_text[0] == '\0') return;
    
    uint32_t start_line = g_notepad.cursor_line;
    uint32_t start_col = g_notepad.cursor_col + 1;
    
    for (uint32_t line = start_line; line < g_notepad.document.line_count; line++) {
        NoteLine* l = &g_notepad.document.lines[line];
        const char* found;
        
        if (line == start_line) {
            found = strstr(&l->text[start_col], g_notepad.search_text);
        } else {
            found = strstr(l->text, g_notepad.search_text);
        }
        
        if (found) {
            g_notepad.cursor_line = line;
            g_notepad.cursor_col = found - l->text;
            
            // Seçim oluştur
            g_notepad.selection.active = true;
            g_notepad.selection.start_line = line;
            g_notepad.selection.start_col = g_notepad.cursor_col;
            g_notepad.selection.end_line = line;
            g_notepad.selection.end_col = g_notepad.cursor_col + strlen(g_notepad.search_text);
            
            notepad_scroll_to_cursor();
            return;
        }
    }
    
    // Baştan ara
    for (uint32_t line = 0; line <= start_line; line++) {
        NoteLine* l = &g_notepad.document.lines[line];
        const char* found = strstr(l->text, g_notepad.search_text);
        
        if (found) {
            uint32_t col = found - l->text;
            if (line < start_line || col < start_col) {
                g_notepad.cursor_line = line;
                g_notepad.cursor_col = col;
                
                g_notepad.selection.active = true;
                g_notepad.selection.start_line = line;
                g_notepad.selection.start_col = col;
                g_notepad.selection.end_line = line;
                g_notepad.selection.end_col = col + strlen(g_notepad.search_text);
                
                notepad_scroll_to_cursor();
                return;
            }
        }
    }
}

// ==================== GÖRÜNÜM ====================

void notepad_scroll_to_cursor(void) {
    // Dikey scroll
    if (g_notepad.cursor_line < g_notepad.scroll_y) {
        g_notepad.scroll_y = g_notepad.cursor_line;
    }
    if (g_notepad.cursor_line >= g_notepad.scroll_y + g_notepad.visible_lines) {
        g_notepad.scroll_y = g_notepad.cursor_line - g_notepad.visible_lines + 1;
    }
    
    // Yatay scroll
    if (g_notepad.cursor_col < g_notepad.scroll_x) {
        g_notepad.scroll_x = g_notepad.cursor_col;
    }
    if (g_notepad.cursor_col >= g_notepad.scroll_x + g_notepad.visible_cols) {
        g_notepad.scroll_x = g_notepad.cursor_col - g_notepad.visible_cols + 1;
    }
}

void notepad_toggle_line_numbers(void) {
    g_notepad.show_line_numbers = !g_notepad.show_line_numbers;
}

// ==================== UI ÇİZİM ====================

void notepad_draw_toolbar(void) {
    int x = g_notepad.window_x;
    int y = g_notepad.window_y;
    int w = g_notepad.window_width;
    int h = g_notepad.toolbar_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_TOOLBAR_BG);
    
    // Butonlar (ikonlar yerine metin)
    int btn_x = x + 10;
    int btn_y = y + 5;
    int btn_w = 30;
    int btn_h = 25;
    int gap = 5;
    
    // Yeni, Aç, Kaydet
    // draw_button(btn_x, btn_y, btn_w, btn_h, "N"); btn_x += btn_w + gap;
    // draw_button(btn_x, btn_y, btn_w, btn_h, "O"); btn_x += btn_w + gap;
    // draw_button(btn_x, btn_y, btn_w, btn_h, "S"); btn_x += btn_w + gap + 10;
    
    // Kes, Kopyala, Yapıştır
    // draw_button(btn_x, btn_y, btn_w, btn_h, "X"); btn_x += btn_w + gap;
    // draw_button(btn_x, btn_y, btn_w, btn_h, "C"); btn_x += btn_w + gap;
    // draw_button(btn_x, btn_y, btn_w, btn_h, "V"); btn_x += btn_w + gap + 10;
    
    // Geri Al, Yinele
    // draw_button(btn_x, btn_y, btn_w, btn_h, "U"); btn_x += btn_w + gap;
    // draw_button(btn_x, btn_y, btn_w, btn_h, "R"); btn_x += btn_w + gap + 10;
    
    // Bul
    // draw_button(btn_x, btn_y, btn_w, btn_h, "F");
}

void notepad_draw_content(void) {
    int x = g_notepad.window_x + g_notepad.content_x;
    int y = g_notepad.window_y + g_notepad.content_y;
    int w = g_notepad.content_width;
    int h = g_notepad.content_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_CONTENT_BG);
    
    int text_x = x + g_notepad.line_number_width + 5;
    int text_y = y;
    int char_height = 16;
    
    // Satırları çiz
    for (uint32_t i = 0; i < g_notepad.visible_lines && 
                         g_notepad.scroll_y + i < g_notepad.document.line_count; i++) {
        
        uint32_t line_index = g_notepad.scroll_y + i;
        
        // Mevcut satır arka planı
        if (line_index == g_notepad.cursor_line) {
            // draw_filled_rect(text_x, text_y, w - g_notepad.line_number_width, char_height, COLOR_CURRENT_LINE);
        }
        
        notepad_draw_line(line_index, text_y);
        text_y += char_height;
    }
}

void notepad_draw_line(uint32_t line_index, int32_t y) {
    NoteLine* line = &g_notepad.document.lines[line_index];
    int x = g_notepad.window_x + g_notepad.content_x + g_notepad.line_number_width + 5;
    int char_width = 8;
    
    // Satır metnini çiz
    for (uint32_t col = g_notepad.scroll_x; col < line->length && 
                                            col < g_notepad.scroll_x + g_notepad.visible_cols; col++) {
        
        char c = line->text[col];
        int cx = x + (col - g_notepad.scroll_x) * char_width;
        
        // Seçim içinde mi?
        bool in_selection = false;
        if (notepad_has_selection()) {
            // Seçim kontrolü...
        }
        
        uint32_t color = in_selection ? COLOR_TEXT_SELECTED : COLOR_TEXT;
        if (in_selection) {
            // draw_filled_rect(cx, y, char_width, 16, COLOR_SELECTION_BG);
        }
        
        // draw_char(cx, y, c, color);
    }
}

void notepad_draw_cursor(void) {
    if (!g_notepad.cursor_visible) return;
    if (g_notepad.cursor_line < g_notepad.scroll_y) return;
    if (g_notepad.cursor_line >= g_notepad.scroll_y + g_notepad.visible_lines) return;
    
    int char_width = 8;
    int char_height = 16;
    
    int x = g_notepad.window_x + g_notepad.content_x + g_notepad.line_number_width + 5;
    int y = g_notepad.window_y + g_notepad.content_y;
    
    int cx = x + (g_notepad.cursor_col - g_notepad.scroll_x) * char_width;
    int cy = y + (g_notepad.cursor_line - g_notepad.scroll_y) * char_height;
    
    // draw_filled_rect(cx, cy, 2, char_height, COLOR_CURSOR);
}

void notepad_draw_line_numbers(void) {
    int x = g_notepad.window_x + g_notepad.content_x;
    int y = g_notepad.window_y + g_notepad.content_y;
    int w = g_notepad.line_number_width;
    int char_height = 16;
    
    // Arka plan
    // draw_filled_rect(x, y, w, g_notepad.content_height, COLOR_LINE_NUMBER_BG);
    
    // Satır numaraları
    for (uint32_t i = 0; i < g_notepad.visible_lines && 
                         g_notepad.scroll_y + i < g_notepad.document.line_count; i++) {
        
        uint32_t line_num = g_notepad.scroll_y + i + 1;
        char num_str[10];
        
        // itoa yerine manuel
        int n = line_num;
        int j = 0;
        char temp[10];
        do {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        
        int k = 0;
        while (j > 0) num_str[k++] = temp[--j];
        num_str[k] = '\0';
        
        int ty = y + i * char_height;
        // draw_text_right(x + w - 10, ty, num_str, COLOR_LINE_NUMBER);
    }
}

void notepad_draw_status_bar(void) {
    int x = g_notepad.window_x;
    int y = g_notepad.window_y + g_notepad.window_height - g_notepad.status_height;
    int w = g_notepad.window_width;
    int h = g_notepad.status_height;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_STATUS_BG);
    
    // Satır ve sütun bilgisi
    char pos_str[32];
    // sprintf yerine manuel: "Satir: X, Sutun: Y"
    // draw_text(x + 10, y + 5, pos_str, COLOR_STATUS_TEXT);
    
    // Dosya durumu
    const char* status = g_notepad.document.modified ? "Degistirildi" : "Kaydedildi";
    // draw_text(x + w - 100, y + 5, status, COLOR_STATUS_TEXT);
    
    // Encoding
    // draw_text(x + w - 200, y + 5, "UTF-8", COLOR_STATUS_TEXT);
}

void notepad_draw_search_bar(void) {
    int x = g_notepad.window_x + 10;
    int y = g_notepad.window_y + g_notepad.toolbar_height + 5;
    int w = g_notepad.window_width - 20;
    int h = 30;
    
    // Arka plan
    // draw_filled_rect(x, y, w, h, COLOR_SEARCH_BG);
    
    // Etiket
    // draw_text(x + 5, y + 8, "Bul:", COLOR_TEXT);
    
    // Arama kutusu
    // draw_filled_rect(x + 40, y + 3, 200, h - 6, 0x2A2A2A);
    // draw_text(x + 45, y + 8, g_notepad.search_text, COLOR_TEXT);
}

void notepad_update_title(void) {
    // Pencere başlığını güncelle
    char title[300];
    if (g_notepad.document.modified) {
        strcpy(title, "*");
    } else {
        title[0] = '\0';
    }
    strcat(title, g_notepad.document.filename);
    strcat(title, " - Notepad");
    
    // Application title güncelle
    // app_set_title(g_notepad.app_id, title);
}
