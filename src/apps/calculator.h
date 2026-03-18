#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "app_manager.h"

/*
 * ============================================
 *          MyOS Calculator
 * ============================================
 * Modern hesap makinesi uygulaması
 * ============================================
 */

// ==================== SABITLER ====================

#define CALC_MAX_DISPLAY    32      // Ekran karakter limiti
#define CALC_MAX_HISTORY    20      // Geçmiş hesaplama sayısı

// ==================== OPERATÖRLER ====================

typedef enum {
    OP_NONE     = 0,
    OP_ADD      = 1,    // +
    OP_SUB      = 2,    // -
    OP_MUL      = 3,    // ×
    OP_DIV      = 4,    // ÷
    OP_MOD      = 5,    // %
    OP_POW      = 6,    // ^
    OP_SQRT     = 7,    // √
    OP_SIN      = 8,
    OP_COS      = 9,
    OP_TAN      = 10,
    OP_LOG      = 11,
    OP_LN       = 12,
} CalcOperator;

// ==================== MODLAR ====================

typedef enum {
    CALC_MODE_BASIC     = 0,    // Temel hesap makinesi
    CALC_MODE_SCIENTIFIC = 1,   // Bilimsel mod
} CalcMode;

// ==================== BUTON ====================

typedef struct {
    int32_t     x, y;           // Konum
    int32_t     width, height;  // Boyut
    char        label[8];       // Etiket
    uint32_t    color;          // Renk
    uint32_t    hover_color;    // Hover rengi
    bool        is_hovered;     // Mouse üzerinde mi?
    void        (*action)(void); // Tıklama aksiyonu
} CalcButton;

// ==================== DURUM ====================

typedef struct {
    // Ekran
    char        display[CALC_MAX_DISPLAY];
    uint32_t    display_length;
    
    // Hesaplama
    double      current_value;
    double      stored_value;
    CalcOperator pending_op;
    bool        new_input;      // Yeni sayı girişi mi?
    bool        has_decimal;    // Ondalık nokta var mı?
    bool        error;          // Hata durumu
    
    // Mod
    CalcMode    mode;
    
    // Bellek
    double      memory;
    bool        memory_set;
    
    // Geçmiş
    char        history[CALC_MAX_HISTORY][64];
    uint32_t    history_count;
    
    // UI
    CalcButton  buttons[40];    // Maksimum buton sayısı
    uint32_t    button_count;
    
    // Uygulama
    uint32_t    app_id;
    int32_t     window_x, window_y;
    int32_t     window_width, window_height;
    
} CalculatorState;

// ==================== FONKSİYONLAR ====================

// Yaşam döngüsü
void            calculator_init(void);
void            calculator_update(void);
void            calculator_render(void);
void            calculator_handle_input(uint8_t key);
void            calculator_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void            calculator_cleanup(void);

// Kayıt
uint32_t        calculator_register(void);

// Sayı girişi
void            calc_input_digit(int digit);
void            calc_input_decimal(void);
void            calc_input_sign(void);

// Operatörler
void            calc_set_operator(CalcOperator op);
void            calc_calculate(void);

// Fonksiyonlar
void            calc_clear(void);           // C
void            calc_clear_entry(void);     // CE
void            calc_backspace(void);       // ←
void            calc_percent(void);         // %
void            calc_sqrt(void);            // √
void            calc_square(void);          // x²
void            calc_reciprocal(void);      // 1/x
void            calc_negate(void);          // ±

// Bilimsel fonksiyonlar
void            calc_sin(void);
void            calc_cos(void);
void            calc_tan(void);
void            calc_log(void);
void            calc_ln(void);
void            calc_pow(void);
void            calc_pi(void);
void            calc_euler(void);
void            calc_factorial(void);

// Bellek
void            calc_memory_clear(void);    // MC
void            calc_memory_recall(void);   // MR
void            calc_memory_add(void);      // M+
void            calc_memory_sub(void);      // M-
void            calc_memory_store(void);    // MS

// Mod
void            calc_toggle_mode(void);

// UI
void            calc_setup_buttons(void);
void            calc_draw_display(void);
void            calc_draw_buttons(void);
void            calc_update_display(void);

// Yardımcı
double          calc_parse_display(void);
void            calc_format_number(double num, char* buffer);
void            calc_add_to_history(const char* expression, double result);

// ==================== GLOBAL ====================

extern CalculatorState g_calculator;

#endif
