#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *            MyOS Terminal
 * ============================================
 * Komut satırı arayüzü
 * ============================================
 */

// ==================== SABITLER ====================

#define TERMINAL_MAX_INPUT      256     // Maksimum komut uzunluğu
#define TERMINAL_MAX_HISTORY    50      // Komut geçmişi sayısı
#define TERMINAL_BUFFER_LINES   1000    // Scroll buffer satır sayısı
#define TERMINAL_WIDTH          80      // Karakter genişliği
#define TERMINAL_HEIGHT         25      // Karakter yüksekliği

// ==================== RENKLER ====================

#define TERM_COLOR_DEFAULT      0x0F    // Beyaz
#define TERM_COLOR_PROMPT       0x0A    // Yeşil
#define TERM_COLOR_ERROR        0x0C    // Kırmızı
#define TERM_COLOR_WARNING      0x0E    // Sarı
#define TERM_COLOR_INFO         0x0B    // Cyan
#define TERM_COLOR_SUCCESS      0x0A    // Yeşil
#define TERM_COLOR_PATH         0x09    // Mavi

// ==================== KOMUT YAPISI ====================

typedef struct {
    const char*     name;           // Komut adı
    const char*     description;    // Açıklama
    const char*     usage;          // Kullanım
    void            (*handler)(int argc, char** argv);  // İşleyici
} TerminalCommand;

// ==================== TERMİNAL DURUMU ====================

typedef struct {
    // Giriş buffer'ı
    char            input_buffer[TERMINAL_MAX_INPUT];
    uint32_t        input_length;
    uint32_t        cursor_pos;
    
    // Komut geçmişi
    char            history[TERMINAL_MAX_HISTORY][TERMINAL_MAX_INPUT];
    uint32_t        history_count;
    int32_t         history_index;      // -1 = mevcut giriş
    
    // Ekran buffer'ı
    char            screen_buffer[TERMINAL_BUFFER_LINES][TERMINAL_WIDTH];
    uint8_t         color_buffer[TERMINAL_BUFFER_LINES][TERMINAL_WIDTH];
    uint32_t        buffer_line_count;
    uint32_t        scroll_offset;
    
    // Cursor
    uint32_t        cursor_x;
    uint32_t        cursor_y;
    bool            cursor_visible;
    uint32_t        cursor_blink_timer;
    
    // Durum
    bool            is_active;
    char            current_path[256];
    char            username[32];
    char            hostname[32];
    
    // Uygulama ID
    uint32_t        app_id;
    
} TerminalState;

// ==================== FONKSİYONLAR ====================

// Uygulama yaşam döngüsü
void            terminal_init(void);
void            terminal_update(void);
void            terminal_render(void);
void            terminal_handle_input(uint8_t key);
void            terminal_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void            terminal_cleanup(void);

// Kayıt
uint32_t        terminal_register(void);

// Çıktı fonksiyonları
void            terminal_print(const char* text, uint8_t color);
void            terminal_println(const char* text, uint8_t color);
void            terminal_printf(uint8_t color, const char* format, ...);
void            terminal_clear(void);
void            terminal_newline(void);

// Giriş fonksiyonları
void            terminal_process_input(void);
void            terminal_add_char(char c);
void            terminal_backspace(void);
void            terminal_delete(void);
void            terminal_cursor_left(void);
void            terminal_cursor_right(void);
void            terminal_cursor_home(void);
void            terminal_cursor_end(void);

// Geçmiş
void            terminal_history_add(const char* command);
void            terminal_history_up(void);
void            terminal_history_down(void);

// Komut işleme
void            terminal_execute(const char* command);
void            terminal_parse_command(const char* input, int* argc, char** argv);

// Prompt
void            terminal_show_prompt(void);

// Tab completion
void            terminal_tab_complete(void);

// ==================== DAHİLİ KOMUTLAR ====================

void            cmd_help(int argc, char** argv);
void            cmd_clear(int argc, char** argv);
void            cmd_echo(int argc, char** argv);
void            cmd_shutdown(int argc, char** argv);
void            cmd_reboot(int argc, char** argv);
void            cmd_version(int argc, char** argv);
void            cmd_uptime(int argc, char** argv);
void            cmd_date(int argc, char** argv);
void            cmd_whoami(int argc, char** argv);
void            cmd_hostname(int argc, char** argv);
void            cmd_pwd(int argc, char** argv);
void            cmd_cd(int argc, char** argv);
void            cmd_ls(int argc, char** argv);
void            cmd_cat(int argc, char** argv);
void            cmd_mkdir(int argc, char** argv);
void            cmd_touch(int argc, char** argv);
void            cmd_rm(int argc, char** argv);
void            cmd_cp(int argc, char** argv);
void            cmd_mv(int argc, char** argv);
void            cmd_ps(int argc, char** argv);
void            cmd_kill(int argc, char** argv);
void            cmd_free(int argc, char** argv);
void            cmd_ping(int argc, char** argv);
void            cmd_ifconfig(int argc, char** argv);
void            cmd_apps(int argc, char** argv);
void            cmd_open(int argc, char** argv);
void            cmd_calc(int argc, char** argv);
void            cmd_browser(int argc, char** argv);
void            cmd_settings(int argc, char** argv);
void            cmd_neofetch(int argc, char** argv);

// ==================== GLOBAL ====================

extern TerminalState g_terminal;
extern TerminalCommand g_terminal_commands[];
extern uint32_t g_terminal_command_count;

#endif
