#include "terminal.h"
#include "../kernel/kernel.h"
#include "../kernel/power.h"
#include <string.h>

/*
 * ============================================
 *            MyOS Terminal
 * ============================================
 */

// ==================== GLOBAL ====================

TerminalState g_terminal;

// ==================== KOMUT TABLOSU ====================

TerminalCommand g_terminal_commands[] = {
    // Sistem
    {"help",        "Yardim mesajini goster",       "help [komut]",         cmd_help},
    {"clear",       "Ekrani temizle",               "clear",                cmd_clear},
    {"shutdown",    "Sistemi kapat",                "shutdown",             cmd_shutdown},
    {"poweroff",    "Sistemi kapat",                "poweroff",             cmd_shutdown},
    {"reboot",      "Yeniden baslat",               "reboot",               cmd_reboot},
    {"restart",     "Yeniden baslat",               "restart",              cmd_reboot},
    {"version",     "Versiyon bilgisi",             "version",              cmd_version},
    {"uptime",      "Calisma suresi",               "uptime",               cmd_uptime},
    {"date",        "Tarih ve saat",                "date",                 cmd_date},
    
    // Kullanıcı
    {"whoami",      "Kullanici adini goster",       "whoami",               cmd_whoami},
    {"hostname",    "Bilgisayar adini goster",      "hostname",             cmd_hostname},
    
    // Dosya sistemi
    {"pwd",         "Mevcut dizini goster",         "pwd",                  cmd_pwd},
    {"cd",          "Dizin degistir",               "cd <dizin>",           cmd_cd},
    {"ls",          "Dizin icerigini listele",      "ls [dizin]",           cmd_ls},
    {"cat",         "Dosya icerigini goster",       "cat <dosya>",          cmd_cat},
    {"mkdir",       "Dizin olustur",                "mkdir <dizin>",        cmd_mkdir},
    {"touch",       "Dosya olustur",                "touch <dosya>",        cmd_touch},
    {"rm",          "Dosya/dizin sil",              "rm <dosya>",           cmd_rm},
    {"cp",          "Kopyala",                      "cp <kaynak> <hedef>",  cmd_cp},
    {"mv",          "Tasi/yeniden adlandir",        "mv <kaynak> <hedef>",  cmd_mv},
    
    // Süreç
    {"ps",          "Surecleri listele",            "ps",                   cmd_ps},
    {"kill",        "Surec sonlandir",              "kill <pid>",           cmd_kill},
    {"free",        "Bellek kullanimi",             "free",                 cmd_free},
    
    // Ağ
    {"ping",        "Ag baglantisi test",           "ping <adres>",         cmd_ping},
    {"ifconfig",    "Ag yapilandirmasi",            "ifconfig",             cmd_ifconfig},
    
    // Uygulamalar
    {"apps",        "Uygulamalari listele",         "apps",                 cmd_apps},
    {"open",        "Uygulama ac",                  "open <uygulama>",      cmd_open},
    {"calc",        "Hesap makinesi",               "calc",                 cmd_calc},
    {"browser",     "Web tarayici",                 "browser [url]",        cmd_browser},
    {"settings",    "Ayarlar",                      "settings",             cmd_settings},
    
    // Eğlence
    {"neofetch",    "Sistem bilgisi (stil)",        "neofetch",             cmd_neofetch},
    {"echo",        "Metin yazdir",                 "echo <metin>",         cmd_echo},
    
    // Son
    {NULL, NULL, NULL, NULL}
};

uint32_t g_terminal_command_count = sizeof(g_terminal_commands) / sizeof(TerminalCommand) - 1;

// ==================== YAŞAM DÖNGÜSÜ ====================

void terminal_init(void) {
    // Durumu sıfırla
    memset(&g_terminal, 0, sizeof(TerminalState));
    
    // Varsayılanlar
    strcpy(g_terminal.current_path, "/");
    strcpy(g_terminal.username, "user");
    strcpy(g_terminal.hostname, "myos");
    
    g_terminal.input_length = 0;
    g_terminal.cursor_pos = 0;
    g_terminal.history_count = 0;
    g_terminal.history_index = -1;
    g_terminal.cursor_x = 0;
    g_terminal.cursor_y = 0;
    g_terminal.cursor_visible = true;
    g_terminal.is_active = true;
    g_terminal.scroll_offset = 0;
    
    // Hoş geldin mesajı
    terminal_clear();
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  __  __        ___  ____    _____                   _             _ ", TERM_COLOR_INFO);
    terminal_println(" |  \\/  |_   _ / _ \\/ ___|  |_   _|__ _ __ _ __ ___ (_)_ __   __ _| |", TERM_COLOR_INFO);
    terminal_println(" | |\\/| | | | | | | \\___ \\    | |/ _ \\ '__| '_ ` _ \\| | '_ \\ / _` | |", TERM_COLOR_INFO);
    terminal_println(" | |  | | |_| | |_| |___) |   | |  __/ |  | | | | | | | | | | (_| | |", TERM_COLOR_INFO);
    terminal_println(" |_|  |_|\\__, |\\___/|____/    |_|\\___|_|  |_| |_| |_|_|_| |_|\\__,_|_|", TERM_COLOR_INFO);
    terminal_println("         |___/                                                       ", TERM_COLOR_INFO);
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  MyOS Terminal v1.0", TERM_COLOR_DEFAULT);
    terminal_println("  'help' yazarak komutlari gorebilirsiniz.", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    
    terminal_show_prompt();
}

void terminal_update(void) {
    // Cursor yanıp sönme
    g_terminal.cursor_blink_timer++;
    if (g_terminal.cursor_blink_timer >= 30) {  // ~500ms
        g_terminal.cursor_visible = !g_terminal.cursor_visible;
        g_terminal.cursor_blink_timer = 0;
    }
}

void terminal_render(void) {
    // GUI modunda pencere içine çiz
    // Text modunda doğrudan ekrana çiz
    
    // Cursor çiz
    if (g_terminal.cursor_visible) {
        // cursor_x, cursor_y konumuna cursor çiz
    }
}

void terminal_handle_input(uint8_t key) {
    // Özel tuşlar
    switch (key) {
        case '\n':  // Enter
        case '\r':
            terminal_newline();
            if (g_terminal.input_length > 0) {
                g_terminal.input_buffer[g_terminal.input_length] = '\0';
                terminal_history_add(g_terminal.input_buffer);
                terminal_execute(g_terminal.input_buffer);
            }
            // Giriş buffer'ını temizle
            memset(g_terminal.input_buffer, 0, TERMINAL_MAX_INPUT);
            g_terminal.input_length = 0;
            g_terminal.cursor_pos = 0;
            g_terminal.history_index = -1;
            terminal_show_prompt();
            return;
            
        case '\b':  // Backspace
            terminal_backspace();
            return;
            
        case '\t':  // Tab
            terminal_tab_complete();
            return;
            
        // TODO: Ok tuşları için scancode kontrolü gerekli
        // case KEY_UP:    terminal_history_up(); return;
        // case KEY_DOWN:  terminal_history_down(); return;
        // case KEY_LEFT:  terminal_cursor_left(); return;
        // case KEY_RIGHT: terminal_cursor_right(); return;
        // case KEY_HOME:  terminal_cursor_home(); return;
        // case KEY_END:   terminal_cursor_end(); return;
        // case KEY_DELETE: terminal_delete(); return;
    }
    
    // Normal karakter
    if (key >= 32 && key < 127) {
        terminal_add_char(key);
    }
}

void terminal_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    // Mouse scroll için
    (void)x;
    (void)y;
    (void)buttons;
}

void terminal_cleanup(void) {
    g_terminal.is_active = false;
    terminal_println("\n[Terminal kapatildi]", TERM_COLOR_WARNING);
}

// ==================== KAYIT ====================

uint32_t terminal_register(void) {
    uint32_t id = app_register("Terminal", "1.0", APP_TYPE_SYSTEM);
    
    Application* app = app_get(id);
    if (app) {
        app->init = terminal_init;
        app->update = terminal_update;
        app->render = terminal_render;
        app->handle_input = terminal_handle_input;
        app->handle_mouse = terminal_handle_mouse;
        app->cleanup = terminal_cleanup;
        
        app->width = 640;
        app->height = 400;
    }
    
    g_terminal.app_id = id;
    return id;
}

// ==================== ÇIKTI FONKSİYONLARI ====================

void terminal_print(const char* text, uint8_t color) {
    // Her karakter için
    while (*text) {
        if (*text == '\n') {
            terminal_newline();
        } else {
            // Karakteri ekrana yaz
            print_char(*text, color);  // kernel.h'dan
            g_terminal.cursor_x++;
            
            if (g_terminal.cursor_x >= TERMINAL_WIDTH) {
                terminal_newline();
            }
        }
        text++;
    }
}

void terminal_println(const char* text, uint8_t color) {
    terminal_print(text, color);
    terminal_newline();
}

void terminal_clear(void) {
    clear_screen();  // kernel.h'dan
    g_terminal.cursor_x = 0;
    g_terminal.cursor_y = 0;
    g_terminal.scroll_offset = 0;
}

void terminal_newline(void) {
    g_terminal.cursor_x = 0;
    g_terminal.cursor_y++;
    
    if (g_terminal.cursor_y >= TERMINAL_HEIGHT) {
        scroll_screen();  // kernel.h'dan
        g_terminal.cursor_y = TERMINAL_HEIGHT - 1;
    }
    
    print("\n", TERM_COLOR_DEFAULT);
}

// ==================== GİRİŞ FONKSİYONLARI ====================

void terminal_add_char(char c) {
    if (g_terminal.input_length >= TERMINAL_MAX_INPUT - 1) return;
    
    // Cursor pozisyonuna ekle
    if (g_terminal.cursor_pos < g_terminal.input_length) {
        // Kaydır
        memmove(&g_terminal.input_buffer[g_terminal.cursor_pos + 1],
                &g_terminal.input_buffer[g_terminal.cursor_pos],
                g_terminal.input_length - g_terminal.cursor_pos);
    }
    
    g_terminal.input_buffer[g_terminal.cursor_pos] = c;
    g_terminal.cursor_pos++;
    g_terminal.input_length++;
    
    // Ekrana yaz
    print_char(c, TERM_COLOR_DEFAULT);
    g_terminal.cursor_x++;
}

void terminal_backspace(void) {
    if (g_terminal.cursor_pos == 0) return;
    
    // Buffer'dan sil
    if (g_terminal.cursor_pos < g_terminal.input_length) {
        memmove(&g_terminal.input_buffer[g_terminal.cursor_pos - 1],
                &g_terminal.input_buffer[g_terminal.cursor_pos],
                g_terminal.input_length - g_terminal.cursor_pos);
    }
    
    g_terminal.cursor_pos--;
    g_terminal.input_length--;
    g_terminal.input_buffer[g_terminal.input_length] = '\0';
    
    // Ekrandan sil
    g_terminal.cursor_x--;
    print_char(' ', TERM_COLOR_DEFAULT);
    g_terminal.cursor_x--;
}

void terminal_delete(void) {
    if (g_terminal.cursor_pos >= g_terminal.input_length) return;
    
    memmove(&g_terminal.input_buffer[g_terminal.cursor_pos],
            &g_terminal.input_buffer[g_terminal.cursor_pos + 1],
            g_terminal.input_length - g_terminal.cursor_pos - 1);
    
    g_terminal.input_length--;
    g_terminal.input_buffer[g_terminal.input_length] = '\0';
}

void terminal_cursor_left(void) {
    if (g_terminal.cursor_pos > 0) {
        g_terminal.cursor_pos--;
        g_terminal.cursor_x--;
    }
}

void terminal_cursor_right(void) {
    if (g_terminal.cursor_pos < g_terminal.input_length) {
        g_terminal.cursor_pos++;
        g_terminal.cursor_x++;
    }
}

void terminal_cursor_home(void) {
    g_terminal.cursor_x -= g_terminal.cursor_pos;
    g_terminal.cursor_pos = 0;
}

void terminal_cursor_end(void) {
    g_terminal.cursor_x += (g_terminal.input_length - g_terminal.cursor_pos);
    g_terminal.cursor_pos = g_terminal.input_length;
}

// ==================== GEÇMİŞ ====================

void terminal_history_add(const char* command) {
    if (strlen(command) == 0) return;
    
    // Son komutla aynı mı?
    if (g_terminal.history_count > 0) {
        if (strcmp(g_terminal.history[g_terminal.history_count - 1], command) == 0) {
            return;  // Aynıysa ekleme
        }
    }
    
    // Yer açma (FIFO)
    if (g_terminal.history_count >= TERMINAL_MAX_HISTORY) {
        memmove(g_terminal.history[0], 
                g_terminal.history[1], 
                (TERMINAL_MAX_HISTORY - 1) * TERMINAL_MAX_INPUT);
        g_terminal.history_count--;
    }
    
    // Ekle
    strcpy(g_terminal.history[g_terminal.history_count], command);
    g_terminal.history_count++;
}

void terminal_history_up(void) {
    if (g_terminal.history_count == 0) return;
    
    if (g_terminal.history_index == -1) {
        g_terminal.history_index = g_terminal.history_count - 1;
    } else if (g_terminal.history_index > 0) {
        g_terminal.history_index--;
    }
    
    // Satırı temizle ve geçmişten yükle
    // TODO: Implement
}

void terminal_history_down(void) {
    if (g_terminal.history_index == -1) return;
    
    if (g_terminal.history_index < (int32_t)g_terminal.history_count - 1) {
        g_terminal.history_index++;
    } else {
        g_terminal.history_index = -1;
        // Boş giriş
    }
    
    // TODO: Implement
}

// ==================== PROMPT ====================

void terminal_show_prompt(void) {
    // [user@myos /]$
    terminal_print("[", TERM_COLOR_DEFAULT);
    terminal_print(g_terminal.username, TERM_COLOR_SUCCESS);
    terminal_print("@", TERM_COLOR_DEFAULT);
    terminal_print(g_terminal.hostname, TERM_COLOR_INFO);
    terminal_print(" ", TERM_COLOR_DEFAULT);
    terminal_print(g_terminal.current_path, TERM_COLOR_PATH);
    terminal_print("]$ ", TERM_COLOR_DEFAULT);
}

// ==================== TAB COMPLETION ====================

void terminal_tab_complete(void) {
    if (g_terminal.input_length == 0) return;
    
    // Mevcut giriş ile başlayan komutları bul
    int matches = 0;
    const char* last_match = NULL;
    
    for (uint32_t i = 0; i < g_terminal_command_count; i++) {
        if (strncmp(g_terminal_commands[i].name, 
                    g_terminal.input_buffer, 
                    g_terminal.input_length) == 0) {
            matches++;
            last_match = g_terminal_commands[i].name;
        }
    }
    
    if (matches == 1 && last_match) {
        // Tek eşleşme, tamamla
        strcpy(g_terminal.input_buffer, last_match);
        g_terminal.input_length = strlen(last_match);
        g_terminal.cursor_pos = g_terminal.input_length;
        
        // Ekranı güncelle
        // TODO: Daha iyi güncelleme
    } else if (matches > 1) {
        // Çoklu eşleşme, listele
        terminal_newline();
        for (uint32_t i = 0; i < g_terminal_command_count; i++) {
            if (strncmp(g_terminal_commands[i].name, 
                        g_terminal.input_buffer, 
                        g_terminal.input_length) == 0) {
                terminal_print(g_terminal_commands[i].name, TERM_COLOR_DEFAULT);
                terminal_print("  ", TERM_COLOR_DEFAULT);
            }
        }
        terminal_newline();
        terminal_show_prompt();
        terminal_print(g_terminal.input_buffer, TERM_COLOR_DEFAULT);
    }
}

// ==================== KOMUT İŞLEME ====================

void terminal_execute(const char* command) {
    // Boşlukları atla
    while (*command == ' ') command++;
    if (*command == '\0') return;
    
    // Komutu ve argümanları ayır
    char cmd_copy[TERMINAL_MAX_INPUT];
    strcpy(cmd_copy, command);
    
    int argc = 0;
    char* argv[16];
    
    char* token = strtok(cmd_copy, " ");
    while (token && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) return;
    
    // Komutu bul ve çalıştır
    for (uint32_t i = 0; i < g_terminal_command_count; i++) {
        if (strcmp(g_terminal_commands[i].name, argv[0]) == 0) {
            g_terminal_commands[i].handler(argc, argv);
            return;
        }
    }
    
    // Komut bulunamadı
    terminal_print("Komut bulunamadi: ", TERM_COLOR_ERROR);
    terminal_println(argv[0], TERM_COLOR_ERROR);
    terminal_println("'help' yazarak mevcut komutlari gorebilirsiniz.", TERM_COLOR_WARNING);
}

// ==================== DAHİLİ KOMUTLAR ====================

void cmd_help(int argc, char** argv) {
    if (argc > 1) {
        // Belirli komut için yardım
        for (uint32_t i = 0; i < g_terminal_command_count; i++) {
            if (strcmp(g_terminal_commands[i].name, argv[1]) == 0) {
                terminal_println("", TERM_COLOR_DEFAULT);
                terminal_print("  Komut: ", TERM_COLOR_INFO);
                terminal_println(g_terminal_commands[i].name, TERM_COLOR_DEFAULT);
                terminal_print("  Aciklama: ", TERM_COLOR_INFO);
                terminal_println(g_terminal_commands[i].description, TERM_COLOR_DEFAULT);
                terminal_print("  Kullanim: ", TERM_COLOR_INFO);
                terminal_println(g_terminal_commands[i].usage, TERM_COLOR_DEFAULT);
                terminal_println("", TERM_COLOR_DEFAULT);
                return;
            }
        }
        terminal_print("Komut bulunamadi: ", TERM_COLOR_ERROR);
        terminal_println(argv[1], TERM_COLOR_ERROR);
        return;
    }
    
    // Tüm komutları listele
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  ========== MyOS Komutlari ==========", TERM_COLOR_INFO);
    terminal_println("", TERM_COLOR_DEFAULT);
    
    terminal_println("  -- Sistem --", TERM_COLOR_WARNING);
    terminal_println("  help        Yardim mesaji", TERM_COLOR_DEFAULT);
    terminal_println("  clear       Ekrani temizle", TERM_COLOR_DEFAULT);
    terminal_println("  shutdown    Sistemi kapat", TERM_COLOR_DEFAULT);
    terminal_println("  reboot      Yeniden baslat", TERM_COLOR_DEFAULT);
    terminal_println("  version     Versiyon bilgisi", TERM_COLOR_DEFAULT);
    terminal_println("  uptime      Calisma suresi", TERM_COLOR_DEFAULT);
    terminal_println("  date        Tarih ve saat", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    
    terminal_println("  -- Dosya Sistemi --", TERM_COLOR_WARNING);
    terminal_println("  pwd         Mevcut dizin", TERM_COLOR_DEFAULT);
    terminal_println("  cd          Dizin degistir", TERM_COLOR_DEFAULT);
    terminal_println("  ls          Dizin listele", TERM_COLOR_DEFAULT);
    terminal_println("  cat         Dosya goster", TERM_COLOR_DEFAULT);
    terminal_println("  mkdir       Dizin olustur", TERM_COLOR_DEFAULT);
    terminal_println("  touch       Dosya olustur", TERM_COLOR_DEFAULT);
    terminal_println("  rm          Sil", TERM_COLOR_DEFAULT);
    terminal_println("  cp          Kopyala", TERM_COLOR_DEFAULT);
    terminal_println("  mv          Tasi", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    
    terminal_println("  -- Ag --", TERM_COLOR_WARNING);
    terminal_println("  ping        Baglanti test", TERM_COLOR_DEFAULT);
    terminal_println("  ifconfig    Ag ayarlari", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    
    terminal_println("  -- Uygulamalar --", TERM_COLOR_WARNING);
    terminal_println("  apps        Uygulama listesi", TERM_COLOR_DEFAULT);
    terminal_println("  open        Uygulama ac", TERM_COLOR_DEFAULT);
    terminal_println("  calc        Hesap makinesi", TERM_COLOR_DEFAULT);
    terminal_println("  browser     Web tarayici", TERM_COLOR_DEFAULT);
    terminal_println("  settings    Ayarlar", TERM_COLOR_DEFAULT);
    terminal_println("  neofetch    Sistem bilgisi", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    
    terminal_println("  'help <komut>' ile detayli bilgi alin.", TERM_COLOR_INFO);
    terminal_println("", TERM_COLOR_DEFAULT);
}

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_clear();
}

void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        terminal_print(argv[i], TERM_COLOR_DEFAULT);
        if (i < argc - 1) terminal_print(" ", TERM_COLOR_DEFAULT);
    }
    terminal_println("", TERM_COLOR_DEFAULT);
}

void cmd_shutdown(int argc, char** argv) {
    (void)argc; (void)argv;
    shutdown();  // power.h'dan
}

void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    reboot();  // power.h'dan
}

void cmd_version(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  MyOS v0.3", TERM_COLOR_INFO);
    terminal_println("  Build: 18.03.2026", TERM_COLOR_DEFAULT);
    terminal_println("  Kernel: 32-bit x86", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
}

void cmd_whoami(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println(g_terminal.username, TERM_COLOR_DEFAULT);
}

void cmd_hostname(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println(g_terminal.hostname, TERM_COLOR_DEFAULT);
}

void cmd_pwd(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println(g_terminal.current_path, TERM_COLOR_DEFAULT);
}

void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        strcpy(g_terminal.current_path, "/");
        return;
    }
    
    // TODO: Gerçek dizin kontrolü (VFS ile)
    if (strcmp(argv[1], "/") == 0 || strcmp(argv[1], "~") == 0) {
        strcpy(g_terminal.current_path, "/");
    } else if (strcmp(argv[1], "..") == 0) {
        // Bir üst dizin
        // TODO: Implement
    } else {
        // Yeni dizin
        strcpy(g_terminal.current_path, argv[1]);
    }
}

void cmd_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    // TODO: VFS ile gerçek dizin listesi
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  bin/", TERM_COLOR_PATH);
    terminal_println("  etc/", TERM_COLOR_PATH);
    terminal_println("  home/", TERM_COLOR_PATH);
    terminal_println("  tmp/", TERM_COLOR_PATH);
    terminal_println("  README.txt", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
}

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: cat <dosya>", TERM_COLOR_ERROR);
        return;
    }
    
    // TODO: VFS ile dosya okuma
    if (strcmp(argv[1], "README.txt") == 0) {
        terminal_println("", TERM_COLOR_DEFAULT);
        terminal_println("MyOS - Hobi Isletim Sistemi", TERM_COLOR_DEFAULT);
        terminal_println("Versiyon 0.3", TERM_COLOR_DEFAULT);
        terminal_println("", TERM_COLOR_DEFAULT);
    } else {
        terminal_print("Dosya bulunamadi: ", TERM_COLOR_ERROR);
        terminal_println(argv[1], TERM_COLOR_ERROR);
    }
}

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: mkdir <dizin>", TERM_COLOR_ERROR);
        return;
    }
    // TODO: VFS ile dizin oluşturma
    terminal_print("Dizin olusturuldu: ", TERM_COLOR_SUCCESS);
    terminal_println(argv[1], TERM_COLOR_DEFAULT);
}

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: touch <dosya>", TERM_COLOR_ERROR);
        return;
    }
    // TODO: VFS ile dosya oluşturma
    terminal_print("Dosya olusturuldu: ", TERM_COLOR_SUCCESS);
    terminal_println(argv[1], TERM_COLOR_DEFAULT);
}

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: rm <dosya>", TERM_COLOR_ERROR);
        return;
    }
    // TODO: VFS ile silme
    terminal_print("Silindi: ", TERM_COLOR_SUCCESS);
    terminal_println(argv[1], TERM_COLOR_DEFAULT);
}

void cmd_cp(int argc, char** argv) {
    if (argc < 3) {
        terminal_println("Kullanim: cp <kaynak> <hedef>", TERM_COLOR_ERROR);
        return;
    }
    // TODO: VFS ile kopyalama
    terminal_print("Kopyalandi: ", TERM_COLOR_SUCCESS);
    terminal_print(argv[1], TERM_COLOR_DEFAULT);
    terminal_print(" -> ", TERM_COLOR_DEFAULT);
    terminal_println(argv[2], TERM_COLOR_DEFAULT);
}

void cmd_mv(int argc, char** argv) {
    if (argc < 3) {
        terminal_println("Kullanim: mv <kaynak> <hedef>", TERM_COLOR_ERROR);
        return;
    }
    // TODO: VFS ile taşıma
    terminal_print("Tasindi: ", TERM_COLOR_SUCCESS);
    terminal_print(argv[1], TERM_COLOR_DEFAULT);
    terminal_print(" -> ", TERM_COLOR_DEFAULT);
    terminal_println(argv[2], TERM_COLOR_DEFAULT);
}

void cmd_ps(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  PID   Ad              Durum", TERM_COLOR_INFO);
    terminal_println("  ---   --              -----", TERM_COLOR_DEFAULT);
    terminal_println("  1     kernel          Running", TERM_COLOR_DEFAULT);
    terminal_println("  2     terminal        Running", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    // TODO: Gerçek process listesi
}

void cmd_kill(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: kill <pid>", TERM_COLOR_ERROR);
        return;
    }
    // TODO: Gerçek process sonlandırma
    terminal_print("Surec sonlandirildi: ", TERM_COLOR_SUCCESS);
    terminal_println(argv[1], TERM_COLOR_DEFAULT);
}

void cmd_free(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  Bellek Kullanimi:", TERM_COLOR_INFO);
    terminal_println("  -----------------", TERM_COLOR_DEFAULT);
    terminal_println("  Toplam:     128 MB", TERM_COLOR_DEFAULT);
    terminal_println("  Kullanilan:  32 MB", TERM_COLOR_DEFAULT);
    terminal_println("  Bos:         96 MB", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    // TODO: Gerçek bellek bilgisi
}

void cmd_ping(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: ping <adres>", TERM_COLOR_ERROR);
        return;
    }
    terminal_print("Ping: ", TERM_COLOR_DEFAULT);
    terminal_println(argv[1], TERM_COLOR_DEFAULT);
    // TODO: Gerçek ICMP ping
    terminal_println("  64 bytes: icmp_seq=1 ttl=64 time=0.1ms", TERM_COLOR_SUCCESS);
    terminal_println("  64 bytes: icmp_seq=2 ttl=64 time=0.1ms", TERM_COLOR_SUCCESS);
    terminal_println("  64 bytes: icmp_seq=3 ttl=64 time=0.1ms", TERM_COLOR_SUCCESS);
}

void cmd_ifconfig(int argc, char** argv) {
    (void)argc; (void)argv;
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("  eth0:", TERM_COLOR_INFO);
    terminal_println("    inet 10.0.2.15  netmask 255.255.255.0", TERM_COLOR_DEFAULT);
    terminal_println("    ether 52:54:00:12:34:56", TERM_COLOR_DEFAULT);
    terminal_println("", TERM_COLOR_DEFAULT);
    // TODO: Gerçek ağ bilgisi
}

void cmd_apps(int argc, char** argv) {
    (void)argc; (void)argv;
    app_manager_list_all();
}

void cmd_open(int argc, char** argv) {
    if (argc < 2) {
        terminal_println("Kullanim: open <uygulama>", TERM_COLOR_ERROR);
        terminal_println("Uygulamalar: calc, browser, settings, notepad, files", TERM_COLOR_INFO);
        return;
    }
    
    if (!app_launch_by_name(argv[1])) {
        terminal_print("Uygulama bulunamadi: ", TERM_COLOR_ERROR);
        terminal_println(argv[1], TERM_COLOR_ERROR);
    }
}

void cmd_calc(int argc, char** argv) {
    (void)argc; (void)argv;
    app_launch_by_name("Calculator");
}

void cmd_browser(int argc, char** argv) {
    (void)argv;
    if (argc > 1) {
        // URL ile aç
        // TODO: browser'a URL gönder
    }
    app_launch_by_name("Browser");
}

void cmd_settings(int argc, char** argv) {
    (void)argc; (void)argv;
    app_launch_by_name("Settings");
}

void cmd_neofetch(int argc, char** argv) {
    (void)argc; (void)argv;
    
    terminal_println("", TERM_COLOR_DEFAULT);
    terminal_println("        ___           user@myos", TERM_COLOR_INFO);
    terminal_println("       /\\  \\          ---------", TERM_COLOR_INFO);
    terminal_print("      /::\\  \\         ", TERM_COLOR_INFO);
    terminal_print("OS: ", TERM_COLOR_DEFAULT);
    terminal_println("MyOS v0.3", TERM_COLOR_DEFAULT);
    terminal_print("     /:/\\:\\  \\        ", TERM_COLOR_INFO);
    terminal_print("Kernel: ", TERM_COLOR_DEFAULT);
    terminal_println("x86 32-bit", TERM_COLOR_DEFAULT);
    terminal_print("    /:/  \\:\\  \\       ", TERM_COLOR_INFO);
    terminal_print("Shell: ", TERM_COLOR_DEFAULT);
    terminal_println("MyOS Terminal", TERM_COLOR_DEFAULT);
    terminal_print("   /:/__/_\\:\\__\\      ", TERM_COLOR_INFO);
    terminal_print("Resolution: ", TERM_COLOR_DEFAULT);
    terminal_println("800x600", TERM_COLOR_DEFAULT);
    terminal_print("   \\:\\  /\\ \\/__/      ", TERM_COLOR_INFO);
    terminal_print("CPU: ", TERM_COLOR_DEFAULT);
    terminal_println("x86 Compatible", TERM_COLOR_DEFAULT);
    terminal_print("    \\:\\ \\:\\__\\        ", TERM_COLOR_INFO);
    terminal_print("Memory: ", TERM_COLOR_DEFAULT);
    terminal_println("128 MB", TERM_COLOR_DEFAULT);
    terminal_println("     \\:\\/:/  /        ", TERM_COLOR_INFO);
    terminal_println("      \\::/  /         ", TERM_COLOR_INFO);
    terminal_println("       \\/__/          ", TERM_COLOR_INFO);
    terminal_println("", TERM_COLOR_DEFAULT);
}

void cmd_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    // TODO: Gerçek uptime hesapla
    terminal_println("Calisma suresi: 0 saat, 5 dakika", TERM_COLOR_DEFAULT);
}

void cmd_date(int argc, char** argv) {
    (void)argc; (void)argv;
    // TODO: RTC'den tarih al
    terminal_println("18 Mart 2026, 14:30:00", TERM_COLOR_DEFAULT);
}
