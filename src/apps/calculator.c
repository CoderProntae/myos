#include "calculator.h"
#include "../kernel/kernel.h"
#include "../gui/window.h"
#include <string.h>
#include <math.h>

/*
 * ============================================
 *          MyOS Calculator
 * ============================================
 */

// ==================== GLOBAL ====================

CalculatorState g_calculator;

// ==================== RENKLER ====================

#define COLOR_BG            0x1E1E1E    // Koyu arka plan
#define COLOR_DISPLAY_BG    0x2D2D2D    // Ekran arka planı
#define COLOR_DISPLAY_TEXT  0xFFFFFF    // Ekran yazı
#define COLOR_BTN_NUMBER    0x3C3C3C    // Sayı butonu
#define COLOR_BTN_OPERATOR  0x505050    // Operatör butonu
#define COLOR_BTN_FUNCTION  0x4A4A4A    // Fonksiyon butonu
#define COLOR_BTN_EQUAL     0x0078D4    // Eşittir butonu (mavi)
#define COLOR_BTN_CLEAR     0xC42B1C    // Temizle butonu (kırmızı)
#define COLOR_BTN_HOVER     0x5A5A5A    // Hover rengi
#define COLOR_BTN_TEXT      0xFFFFFF    // Buton yazı
#define COLOR_MEMORY        0xFFA500    // Bellek göstergesi

// ==================== YAŞAM DÖNGÜSÜ ====================

void calculator_init(void) {
    memset(&g_calculator, 0, sizeof(CalculatorState));
    
    // Varsayılanlar
    strcpy(g_calculator.display, "0");
    g_calculator.display_length = 1;
    g_calculator.current_value = 0;
    g_calculator.stored_value = 0;
    g_calculator.pending_op = OP_NONE;
    g_calculator.new_input = true;
    g_calculator.has_decimal = false;
    g_calculator.error = false;
    g_calculator.mode = CALC_MODE_BASIC;
    g_calculator.memory = 0;
    g_calculator.memory_set = false;
    g_calculator.history_count = 0;
    
    // Pencere boyutu
    g_calculator.window_width = 320;
    g_calculator.window_height = 480;
    g_calculator.window_x = 100;
    g_calculator.window_y = 100;
    
    // Butonları ayarla
    calc_setup_buttons();
}

void calculator_update(void) {
    // Animasyonlar, hover kontrolü vs.
}

void calculator_render(void) {
    // Arka plan
    // draw_filled_rect(g_calculator.window_x, g_calculator.window_y,
    //                  g_calculator.window_width, g_calculator.window_height,
    //                  COLOR_BG);
    
    // Ekran
    calc_draw_display();
    
    // Butonlar
    calc_draw_buttons();
    
    // Bellek göstergesi
    if (g_calculator.memory_set) {
        // draw_text(g_calculator.window_x + 10, g_calculator.window_y + 10,
        //           "M", COLOR_MEMORY);
    }
}

void calculator_handle_input(uint8_t key) {
    // Sayılar
    if (key >= '0' && key <= '9') {
        calc_input_digit(key - '0');
        return;
    }
    
    switch (key) {
        // Operatörler
        case '+':   calc_set_operator(OP_ADD); break;
        case '-':   calc_set_operator(OP_SUB); break;
        case '*':   calc_set_operator(OP_MUL); break;
        case '/':   calc_set_operator(OP_DIV); break;
        case '%':   calc_percent(); break;
        case '^':   calc_set_operator(OP_POW); break;
        
        // Hesaplama
        case '=':
        case '\n':
        case '\r':
            calc_calculate();
            break;
        
        // Düzenleme
        case '\b':  calc_backspace(); break;
        case 'c':
        case 'C':   calc_clear(); break;
        case '.':   calc_input_decimal(); break;
        
        // Escape - kapat
        case 27:
            app_close(g_calculator.app_id);
            break;
    }
}

void calculator_handle_mouse(int32_t x, int32_t y, uint8_t buttons) {
    // Pencere içi koordinata çevir
    int32_t local_x = x - g_calculator.window_x;
    int32_t local_y = y - g_calculator.window_y;
    
    // Butonları kontrol et
    for (uint32_t i = 0; i < g_calculator.button_count; i++) {
        CalcButton* btn = &g_calculator.buttons[i];
        
        // Hover kontrolü
        bool in_button = (local_x >= btn->x && local_x < btn->x + btn->width &&
                          local_y >= btn->y && local_y < btn->y + btn->height);
        
        btn->is_hovered = in_button;
        
        // Tıklama
        if (in_button && (buttons & 1) && btn->action) {
            btn->action();
        }
    }
}

void calculator_cleanup(void) {
    // Temizlik
}

// ==================== KAYIT ====================

uint32_t calculator_register(void) {
    uint32_t id = app_register("Calculator", "2.0", APP_TYPE_UTILITY);
    
    Application* app = app_get(id);
    if (app) {
        app->init = calculator_init;
        app->update = calculator_update;
        app->render = calculator_render;
        app->handle_input = calculator_handle_input;
        app->handle_mouse = calculator_handle_mouse;
        app->cleanup = calculator_cleanup;
        
        app->width = 320;
        app->height = 480;
    }
    
    g_calculator.app_id = id;
    return id;
}

// ==================== SAYI GİRİŞİ ====================

void calc_input_digit(int digit) {
    if (g_calculator.error) {
        calc_clear();
    }
    
    if (g_calculator.new_input) {
        // Yeni sayı başlat
        g_calculator.display[0] = '0' + digit;
        g_calculator.display[1] = '\0';
        g_calculator.display_length = 1;
        g_calculator.new_input = false;
        g_calculator.has_decimal = false;
    } else {
        // Mevcut sayıya ekle
        if (g_calculator.display_length < CALC_MAX_DISPLAY - 1) {
            // Baştaki 0'ı kontrol et
            if (g_calculator.display_length == 1 && 
                g_calculator.display[0] == '0' && 
                !g_calculator.has_decimal) {
                g_calculator.display[0] = '0' + digit;
            } else {
                g_calculator.display[g_calculator.display_length] = '0' + digit;
                g_calculator.display_length++;
                g_calculator.display[g_calculator.display_length] = '\0';
            }
        }
    }
    
    calc_update_display();
}

void calc_input_decimal(void) {
    if (g_calculator.error) {
        calc_clear();
    }
    
    if (g_calculator.new_input) {
        strcpy(g_calculator.display, "0.");
        g_calculator.display_length = 2;
        g_calculator.new_input = false;
        g_calculator.has_decimal = true;
    } else if (!g_calculator.has_decimal) {
        if (g_calculator.display_length < CALC_MAX_DISPLAY - 1) {
            g_calculator.display[g_calculator.display_length] = '.';
            g_calculator.display_length++;
            g_calculator.display[g_calculator.display_length] = '\0';
            g_calculator.has_decimal = true;
        }
    }
    
    calc_update_display();
}

// ==================== OPERATÖRLER ====================

void calc_set_operator(CalcOperator op) {
    if (g_calculator.error) return;
    
    // Önceki işlem varsa hesapla
    if (g_calculator.pending_op != OP_NONE && !g_calculator.new_input) {
        calc_calculate();
    }
    
    g_calculator.stored_value = calc_parse_display();
    g_calculator.pending_op = op;
    g_calculator.new_input = true;
}

void calc_calculate(void) {
    if (g_calculator.error) return;
    if (g_calculator.pending_op == OP_NONE) return;
    
    double current = calc_parse_display();
    double result = 0;
    
    switch (g_calculator.pending_op) {
        case OP_ADD:
            result = g_calculator.stored_value + current;
            break;
        case OP_SUB:
            result = g_calculator.stored_value - current;
            break;
        case OP_MUL:
            result = g_calculator.stored_value * current;
            break;
        case OP_DIV:
            if (current == 0) {
                g_calculator.error = true;
                strcpy(g_calculator.display, "Hata: 0'a bolme");
                g_calculator.display_length = strlen(g_calculator.display);
                return;
            }
            result = g_calculator.stored_value / current;
            break;
        case OP_MOD:
            if (current == 0) {
                g_calculator.error = true;
                strcpy(g_calculator.display, "Hata");
                return;
            }
            result = (int)g_calculator.stored_value % (int)current;
            break;
        case OP_POW:
            result = 1;
            for (int i = 0; i < (int)current; i++) {
                result *= g_calculator.stored_value;
            }
            break;
        default:
            return;
    }
    
    // Geçmişe ekle
    char expr[64];
    // sprintf(expr, "%g %c %g = %g", stored, op_char, current, result);
    // calc_add_to_history(expr, result);
    
    // Sonucu göster
    calc_format_number(result, g_calculator.display);
    g_calculator.display_length = strlen(g_calculator.display);
    g_calculator.current_value = result;
    g_calculator.stored_value = result;
    g_calculator.pending_op = OP_NONE;
    g_calculator.new_input = true;
    g_calculator.has_decimal = (strchr(g_calculator.display, '.') != NULL);
    
    calc_update_display();
}

// ==================== FONKSİYONLAR ====================

void calc_clear(void) {
    strcpy(g_calculator.display, "0");
    g_calculator.display_length = 1;
    g_calculator.current_value = 0;
    g_calculator.stored_value = 0;
    g_calculator.pending_op = OP_NONE;
    g_calculator.new_input = true;
    g_calculator.has_decimal = false;
    g_calculator.error = false;
    calc_update_display();
}

void calc_clear_entry(void) {
    strcpy(g_calculator.display, "0");
    g_calculator.display_length = 1;
    g_calculator.new_input = true;
    g_calculator.has_decimal = false;
    g_calculator.error = false;
    calc_update_display();
}

void calc_backspace(void) {
    if (g_calculator.error) {
        calc_clear();
        return;
    }
    
    if (g_calculator.new_input) return;
    
    if (g_calculator.display_length > 1) {
        char last = g_calculator.display[g_calculator.display_length - 1];
        if (last == '.') {
            g_calculator.has_decimal = false;
        }
        g_calculator.display_length--;
        g_calculator.display[g_calculator.display_length] = '\0';
    } else {
        strcpy(g_calculator.display, "0");
        g_calculator.new_input = true;
    }
    
    calc_update_display();
}

void calc_percent(void) {
    if (g_calculator.error) return;
    
    double current = calc_parse_display();
    double result;
    
    if (g_calculator.pending_op == OP_ADD || g_calculator.pending_op == OP_SUB) {
        result = g_calculator.stored_value * (current / 100.0);
    } else {
        result = current / 100.0;
    }
    
    calc_format_number(result, g_calculator.display);
    g_calculator.display_length = strlen(g_calculator.display);
    calc_update_display();
}

void calc_sqrt(void) {
    if (g_calculator.error) return;
    
    double current = calc_parse_display();
    if (current < 0) {
        g_calculator.error = true;
        strcpy(g_calculator.display, "Hata");
        return;
    }
    
    // Basit sqrt implementasyonu (Newton-Raphson)
    double result = current;
    for (int i = 0; i < 20; i++) {
        result = (result + current / result) / 2.0;
    }
    
    calc_format_number(result, g_calculator.display);
    g_calculator.display_length = strlen(g_calculator.display);
    g_calculator.new_input = true;
    calc_update_display();
}

void calc_square(void) {
    if (g_calculator.error) return;
    
    double current = calc_parse_display();
    double result = current * current;
    
    calc_format_number(result, g_calculator.display);
    g_calculator.display_length = strlen(g_calculator.display);
    g_calculator.new_input = true;
    calc_update_display();
}

void calc_negate(void) {
    if (g_calculator.error) return;
    if (strcmp(g_calculator.display, "0") == 0) return;
    
    if (g_calculator.display[0] == '-') {
        // Negatiften pozitife
        memmove(g_calculator.display, g_calculator.display + 1, 
                g_calculator.display_length);
        g_calculator.display_length--;
    } else {
        // Pozitiften negatife
        memmove(g_calculator.display + 1, g_calculator.display, 
                g_calculator.display_length + 1);
        g_calculator.display[0] = '-';
        g_calculator.display_length++;
    }
    
    calc_update_display();
}

void calc_reciprocal(void) {
    if (g_calculator.error) return;
    
    double current = calc_parse_display();
    if (current == 0) {
        g_calculator.error = true;
        strcpy(g_calculator.display, "Hata: 0'a bolme");
        return;
    }
    
    double result = 1.0 / current;
    calc_format_number(result, g_calculator.display);
    g_calculator.display_length = strlen(g_calculator.display);
    g_calculator.new_input = true;
    calc_update_display();
}

// ==================== BELLEK ====================

void calc_memory_clear(void) {
    g_calculator.memory = 0;
    g_calculator.memory_set = false;
}

void calc_memory_recall(void) {
    if (!g_calculator.memory_set) return;
    
    calc_format_number(g_calculator.memory, g_calculator.display);
    g_calculator.display_length = strlen(g_calculator.display);
    g_calculator.new_input = true;
    calc_update_display();
}

void calc_memory_add(void) {
    double current = calc_parse_display();
    g_calculator.memory += current;
    g_calculator.memory_set = true;
    g_calculator.new_input = true;
}

void calc_memory_sub(void) {
    double current = calc_parse_display();
    g_calculator.memory -= current;
    g_calculator.memory_set = true;
    g_calculator.new_input = true;
}

void calc_memory_store(void) {
    g_calculator.memory = calc_parse_display();
    g_calculator.memory_set = true;
    g_calculator.new_input = true;
}

// ==================== MOD ====================

void calc_toggle_mode(void) {
    if (g_calculator.mode == CALC_MODE_BASIC) {
        g_calculator.mode = CALC_MODE_SCIENTIFIC;
        g_calculator.window_width = 480;  // Daha geniş
    } else {
        g_calculator.mode = CALC_MODE_BASIC;
        g_calculator.window_width = 320;
    }
    calc_setup_buttons();
}

// ==================== UI ====================

void calc_setup_buttons(void) {
    g_calculator.button_count = 0;
    
    int start_y = 120;  // Ekranın altından başla
    int btn_w = 70;
    int btn_h = 60;
    int gap = 5;
    int margin = 10;
    
    // Temel mod butonları (4x5 grid)
    const char* basic_labels[] = {
        "C",  "CE", "<-", "/",
        "7",  "8",  "9",  "*",
        "4",  "5",  "6",  "-",
        "1",  "2",  "3",  "+",
        "+/-","0",  ".",  "="
    };
    
    void (*basic_actions[])(void) = {
        calc_clear, calc_clear_entry, calc_backspace, NULL,  // / -> set_operator
        NULL, NULL, NULL, NULL,  // 7,8,9,* 
        NULL, NULL, NULL, NULL,  // 4,5,6,-
        NULL, NULL, NULL, NULL,  // 1,2,3,+
        calc_negate, NULL, calc_input_decimal, calc_calculate
    };
    
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int idx = row * 4 + col;
            CalcButton* btn = &g_calculator.buttons[g_calculator.button_count];
            
            btn->x = margin + col * (btn_w + gap);
            btn->y = start_y + row * (btn_h + gap);
            btn->width = btn_w;
            btn->height = btn_h;
            strcpy(btn->label, basic_labels[idx]);
            btn->is_hovered = false;
            
            // Renk ata
            if (strcmp(btn->label, "C") == 0) {
                btn->color = COLOR_BTN_CLEAR;
            } else if (strcmp(btn->label, "=") == 0) {
                btn->color = COLOR_BTN_EQUAL;
            } else if (btn->label[0] >= '0' && btn->label[0] <= '9') {
                btn->color = COLOR_BTN_NUMBER;
            } else {
                btn->color = COLOR_BTN_OPERATOR;
            }
            btn->hover_color = COLOR_BTN_HOVER;
            
            // Aksiyon ata
            btn->action = basic_actions[idx];
            
            g_calculator.button_count++;
        }
    }
}

void calc_draw_display(void) {
    // Ekran arka planı
    int display_x = g_calculator.window_x + 10;
    int display_y = g_calculator.window_y + 40;
    int display_w = g_calculator.window_width - 20;
    int display_h = 70;
    
    // draw_filled_rect(display_x, display_y, display_w, display_h, COLOR_DISPLAY_BG);
    
    // Sayı (sağa hizalı)
    // draw_text_right(display_x + display_w - 10, display_y + 20,
    //                 g_calculator.display, COLOR_DISPLAY_TEXT, FONT_LARGE);
    
    // Pending operatör göster
    if (g_calculator.pending_op != OP_NONE) {
        char op_char = ' ';
        switch (g_calculator.pending_op) {
            case OP_ADD: op_char = '+'; break;
            case OP_SUB: op_char = '-'; break;
            case OP_MUL: op_char = '*'; break;
            case OP_DIV: op_char = '/'; break;
            default: break;
        }
        // draw_char(display_x + 10, display_y + 10, op_char, COLOR_DISPLAY_TEXT);
    }
}

void calc_draw_buttons(void) {
    for (uint32_t i = 0; i < g_calculator.button_count; i++) {
        CalcButton* btn = &g_calculator.buttons[i];
        
        int x = g_calculator.window_x + btn->x;
        int y = g_calculator.window_y + btn->y;
        uint32_t color = btn->is_hovered ? btn->hover_color : btn->color;
        
        // Buton arka planı (yuvarlak köşeli)
        // draw_rounded_rect(x, y, btn->width, btn->height, 8, color);
        
        // Etiket (ortala)
        // draw_text_centered(x + btn->width/2, y + btn->height/2,
        //                    btn->label, COLOR_BTN_TEXT);
    }
}

void calc_update_display(void) {
    // Ekranı yeniden çiz
    calc_draw_display();
}

// ==================== YARDIMCI ====================

double calc_parse_display(void) {
    // Basit string to double
    double result = 0;
    double decimal = 0.1;
    bool negative = false;
    bool in_decimal = false;
    
    const char* s = g_calculator.display;
    
    if (*s == '-') {
        negative = true;
        s++;
    }
    
    while (*s) {
        if (*s == '.') {
            in_decimal = true;
        } else if (*s >= '0' && *s <= '9') {
            if (in_decimal) {
                result += (*s - '0') * decimal;
                decimal *= 0.1;
            } else {
                result = result * 10 + (*s - '0');
            }
        }
        s++;
    }
    
    return negative ? -result : result;
}

void calc_format_number(double num, char* buffer) {
    // Basit formatlama
    // TODO: Daha iyi implementasyon
    
    if (num == (int)num) {
        // Tam sayı
        int n = (int)num;
        if (n == 0) {
            buffer[0] = '0';
            buffer[1] = '\0';
            return;
        }
        
        bool negative = n < 0;
        if (negative) n = -n;
        
        char temp[32];
        int i = 0;
        while (n > 0) {
            temp[i++] = '0' + (n % 10);
            n /= 10;
        }
        
        int j = 0;
        if (negative) buffer[j++] = '-';
        while (i > 0) {
            buffer[j++] = temp[--i];
        }
        buffer[j] = '\0';
    } else {
        // Ondalık sayı - basit implementasyon
        // TODO: sprintf olmadan düzgün formatlama
        int integer_part = (int)num;
        double decimal_part = num - integer_part;
        if (decimal_part < 0) decimal_part = -decimal_part;
        
        calc_format_number(integer_part, buffer);
        int len = strlen(buffer);
        buffer[len++] = '.';
        
        // 6 ondalık basamak
        for (int i = 0; i < 6; i++) {
            decimal_part *= 10;
            int digit = (int)decimal_part;
            buffer[len++] = '0' + digit;
            decimal_part -= digit;
        }
        
        // Sondaki sıfırları kaldır
        while (len > 1 && buffer[len-1] == '0') len--;
        if (buffer[len-1] == '.') len--;
        buffer[len] = '\0';
    }
}

void calc_add_to_history(const char* expression, double result) {
    (void)result;
    
    if (g_calculator.history_count >= CALC_MAX_HISTORY) {
        // En eskiyi sil
        memmove(g_calculator.history[0], g_calculator.history[1],
                (CALC_MAX_HISTORY - 1) * 64);
        g_calculator.history_count--;
    }
    
    strcpy(g_calculator.history[g_calculator.history_count], expression);
    g_calculator.history_count++;
}
