#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *           MyOS Notepad
 * ============================================
 * Basit metin editörü
 * ============================================
 */

// ==================== SABITLER ====================

#define NOTEPAD_MAX_LINES       10000   // Maksimum satır sayısı
#define NOTEPAD_MAX_LINE_LEN    1024    // Satır başına maksimum karakter
#define NOTEPAD_MAX_UNDO        100     // Geri alma adımı
#define NOTEPAD_TAB_SIZE        4       // Tab genişliği

// ==================== UNDO TİPLERİ ====================

typedef enum {
    UNDO_INSERT_CHAR    = 0,
    UNDO_DELETE_CHAR    = 1,
    UNDO_INSERT_LINE    = 2,
    UNDO_DELETE_LINE    = 3,
    UNDO_REPLACE        = 4,
} UndoType;

// ==================== UNDO KAYDI ====================

typedef struct {
    UndoType        type;
    uint32_t        line;
    uint32_t        column;
    char            data[NOTEPAD_MAX_LINE_LEN];
    uint32_t        data_length;
} UndoRecord;

// ==================== SATIR ====================

typedef struct {
    char            text[NOTEPAD_MAX_LINE_LEN];
    uint32_t        length;
} NoteLine;

// ==================== BELGE ====================

typedef struct {
    // Satırlar
    NoteLine        lines[NOTEPAD_MAX_LINES];
    uint32_t        line_count;
    
    // Dosya bilgisi
    char            filename[256];
    char            filepath[512];
    bool            modified;
    bool            new_file;
    
    // Encoding
    bool            utf8;
    
} NoteDocument;

// ==================== SEÇİM ====================

typedef struct {
    bool            active;
    uint32_t        start_line;
    uint32_t        start_col;
    uint32_t        end_line;
    uint32_t        end_col;
} NoteSelection;

// ==================== NOTEPAD DURUMU ====================

typedef struct {
    // Belge
    NoteDocument    document;
    
    // Cursor
    uint32_t        cursor_line;
    uint32_t        cursor_col;
    uint32_t        desired_col;        // Dikey hareket için
    bool            cursor_visible;
    uint32_t        cursor_blink;
    
    // Seçim
    NoteSelection   selection;
    
    // Görünüm
    uint32_t        scroll_x;
    uint32_t        scroll_y;           // İlk görünen satır
    uint32_t        visible_lines;
    uint32_t        visible_cols;
    
    // Undo/Redo
    UndoRecord      undo_stack[NOTEPAD_MAX_UNDO];
    uint32_t        undo_count;
    uint32_t        undo_index;
    
    // Arama
    bool            search_active;
    char            search_text[256];
    uint32_t        search_cursor;
    bool            search_case_sensitive;
    
    // Değiştir
    bool            replace_active;
    char            replace_text[256];
    
    // Satır numaraları
    bool            show_line_numbers;
    uint32_t        line_number_width;
    
    // Durum çubuğu
    bool            show_status_bar;
    
    // Word wrap
    bool            word_wrap;
    
    // Pencere
    uint32_t        app_id;
    int32_t         window_x, window_y;
    int32_t         window_width, window_height;
    
    // UI boyutları
    int32_t         menu_height;
    int32_t         toolbar_height;
    int32_t         status_height;
    int32_t         content_x, content_y;
    int32_t         content_width, content_height;
    
} NotepadState;

// ==================== FONKSİYONLAR ====================

// Yaşam döngüsü
void            notepad_init(void);
void            notepad_update(void);
void            notepad_render(void);
void            notepad_handle_input(uint8_t key);
void            notepad_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void            notepad_cleanup(void);

// Kayıt
uint32_t        notepad_register(void);

// Dosya işlemleri
void            notepad_new(void);
void            notepad_open(const char* filepath);
void            notepad_save(void);
void            notepad_save_as(const char* filepath);
bool            notepad_close(void);

// Metin düzenleme
void            notepad_insert_char(char c);
void            notepad_insert_text(const char* text);
void            notepad_insert_newline(void);
void            notepad_delete_char(void);          // Delete
void            notepad_backspace(void);            // Backspace
void            notepad_delete_line(void);
void            notepad_delete_selection(void);

// Cursor hareketi
void            notepad_cursor_left(void);
void            notepad_cursor_right(void);
void            notepad_cursor_up(void);
void            notepad_cursor_down(void);
void            notepad_cursor_home(void);          // Satır başı
void            notepad_cursor_end(void);           // Satır sonu
void            notepad_cursor_page_up(void);
void            notepad_cursor_page_down(void);
void            notepad_cursor_doc_start(void);     // Ctrl+Home
void            notepad_cursor_doc_end(void);       // Ctrl+End
void            notepad_cursor_word_left(void);     // Ctrl+Left
void            notepad_cursor_word_right(void);    // Ctrl+Right
void            notepad_goto_line(uint32_t line);

// Seçim
void            notepad_select_all(void);
void            notepad_select_word(void);
void            notepad_select_line(void);
void            notepad_start_selection(void);
void            notepad_update_selection(void);
void            notepad_clear_selection(void);
bool            notepad_has_selection(void);
char*           notepad_get_selection_text(char* buffer, uint32_t max_size);

// Pano (Clipboard)
void            notepad_cut(void);
void            notepad_copy(void);
void            notepad_paste(void);

// Undo/Redo
void            notepad_undo(void);
void            notepad_redo(void);
void            notepad_push_undo(UndoType type, uint32_t line, uint32_t col, const char* data);

// Arama
void            notepad_find(void);
void            notepad_find_next(void);
void            notepad_find_prev(void);
void            notepad_replace(void);
void            notepad_replace_all(void);

// Görünüm
void            notepad_scroll_to_cursor(void);
void            notepad_scroll_up(uint32_t lines);
void            notepad_scroll_down(uint32_t lines);
void            notepad_toggle_line_numbers(void);
void            notepad_toggle_word_wrap(void);
void            notepad_toggle_status_bar(void);

// UI çizim
void            notepad_draw_menu(void);
void            notepad_draw_toolbar(void);
void            notepad_draw_content(void);
void            notepad_draw_line(uint32_t line_index, int32_t y);
void            notepad_draw_cursor(void);
void            notepad_draw_selection(void);
void            notepad_draw_line_numbers(void);
void            notepad_draw_status_bar(void);
void            notepad_draw_search_bar(void);

// Yardımcı
uint32_t        notepad_get_line_width(uint32_t line);
void            notepad_update_title(void);

// ==================== GLOBAL ====================

extern NotepadState g_notepad;

#endif
