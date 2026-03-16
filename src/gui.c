#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"

static int start_open = 0;
static int window_open = 0;
static int settings_selected_depth = 32;

static uint8_t bcd2bin(uint8_t b) { return ((b>>4)*10)+(b&0x0F); }

static void get_time(char* buf) {
    outb(0x70,0x04); uint8_t h=bcd2bin(inb(0x71));
    outb(0x70,0x02); uint8_t m=bcd2bin(inb(0x71));
    buf[0]='0'+h/10; buf[1]='0'+h%10; buf[2]=':';
    buf[3]='0'+m/10; buf[4]='0'+m%10; buf[5]='\0';
}

/* ======== GRAFIK TERMINAL ======== */
#define GT_COLS 88
#define GT_ROWS 30
#define GT_OX   28
#define GT_OY   50

static char gt_buf[GT_ROWS][GT_COLS + 1];
static int gt_cx, gt_cy;
static char gt_cmd[256];
static int gt_cmd_len;
static int gt_prompt_x;

static void gt_clear_buf(void) {
    for (int r = 0; r < GT_ROWS; r++)
        k_memset(gt_buf[r], 0, GT_COLS + 1);
    gt_cx = 0; gt_cy = 0;
}

static void gt_scroll(void) {
    while (gt_cy >= GT_ROWS) {
        for (int r = 0; r < GT_ROWS - 1; r++)
            k_memset(gt_buf[r], 0, GT_COLS + 1);
        for (int r = 0; r < GT_ROWS - 1; r++)
            for (int c = 0; c <= GT_COLS; c++)
                gt_buf[r][c] = gt_buf[r + 1][c];
        k_memset(gt_buf[GT_ROWS - 1], 0, GT_COLS + 1);
        gt_cy--;
    }
}

static void gt_putc(char c) {
    if (c == '\n') {
        gt_cx = 0; gt_cy++;
        gt_scroll();
    } else if (c == '\b') {
        if (gt_cx > 0) { gt_cx--; gt_buf[gt_cy][gt_cx] = ' '; }
    } else if (gt_cx < GT_COLS) {
        gt_buf[gt_cy][gt_cx] = c;
        gt_cx++;
        if (gt_cx >= GT_COLS) { gt_cx = 0; gt_cy++; gt_scroll(); }
    }
}

static void gt_puts(const char* s) {
    while (*s) gt_putc(*s++);
}

static void gt_render(void) {
    int sw = vesa_get_width(), sh = vesa_get_height();
    int wx = 20, wy = 10, ww = sw - 40, wh = sh - 50;

    vesa_fill_screen(COLOR_BG);

    /* Pencere */
    vesa_fill_rect(wx, wy, ww, wh, COLOR_TERMINAL_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);

    /* Baslik */
    vesa_fill_rect(wx+8, wy+6, 20, 20, 0x333355);
    vesa_fill_rect(wx+9, wy+7, 18, 18, COLOR_TERMINAL_BG);
    vesa_fill_rect(wx+12, wy+12, 2, 2, COLOR_TEXT_GREEN);
    vesa_fill_rect(wx+14, wy+14, 2, 2, COLOR_TEXT_GREEN);
    vesa_fill_rect(wx+12, wy+16, 2, 2, COLOR_TEXT_GREEN);
    vesa_fill_rect(wx+17, wy+14, 5, 2, COLOR_TEXT_GREY);
    vesa_draw_string(wx+36, wy+8, "Terminal - MyOS", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    /* X butonu */
    vesa_fill_rect(wx+ww-28, wy+4, 24, 24, COLOR_CLOSE_BTN);
    for (int i = 0; i < 10; i++) {
        vesa_putpixel(wx+ww-22+i, wy+10+i, COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-13-i, wy+10+i, COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-21+i, wy+10+i, COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-14-i, wy+10+i, COLOR_TEXT_WHITE);
    }

    /* Metin */
    for (int r = 0; r < GT_ROWS; r++) {
        for (int c = 0; c < GT_COLS; c++) {
            char ch = gt_buf[r][c];
            if (ch && ch != ' ') {
                uint32_t fg = COLOR_TEXT_GREY;
                /* Prompt renklendirme - satirin basindaki myos:~$ */
                if (c < 8) fg = COLOR_TEXT_GREEN;
                vesa_draw_char(GT_OX + c * 8, GT_OY + r * 16, ch, fg, COLOR_TERMINAL_BG);
            }
        }
    }

    /* Cursor */
    vesa_fill_rect(GT_OX + gt_cx * 8, GT_OY + gt_cy * 16, 8, 16, 0x888888);

    vesa_copy_buffer();
}

static void gt_prompt(void) {
    gt_puts("myos:~$ ");
    gt_prompt_x = gt_cx;
    gt_cmd_len = 0;
    k_memset(gt_cmd, 0, 256);
}

static int gt_process_cmd(void) {
    gt_putc('\n');

    /* Bosluk atla */
    const char* c = gt_cmd;
    while (*c == ' ') c++;

    if (k_strlen(c) == 0) { return 0; }
    if (k_strcmp(c, "exit") == 0) { return 1; }
    if (k_strcmp(c, "clear") == 0) { gt_clear_buf(); return 0; }
    if (k_strcmp(c, "help") == 0) {
        gt_puts("  Komutlar:\n");
        gt_puts("  help     - Bu mesaj\n");
        gt_puts("  clear    - Ekrani temizle\n");
        gt_puts("  echo     - Yazi yazdir\n");
        gt_puts("  time     - Saat\n");
        gt_puts("  sysinfo  - Sistem bilgisi\n");
        gt_puts("  exit     - Masaustune don\n");
        return 0;
    }
    if (k_strcmp(c, "time") == 0) {
        outb(0x70,0x04); uint8_t h = bcd2bin(inb(0x71));
        outb(0x70,0x02); uint8_t m = bcd2bin(inb(0x71));
        outb(0x70,0x00); uint8_t s = bcd2bin(inb(0x71));
        char tb[12];
        tb[0]=' '; tb[1]=' ';
        tb[2]='0'+h/10; tb[3]='0'+h%10; tb[4]=':';
        tb[5]='0'+m/10; tb[6]='0'+m%10; tb[7]=':';
        tb[8]='0'+s/10; tb[9]='0'+s%10; tb[10]='\n'; tb[11]='\0';
        gt_puts(tb);
        return 0;
    }
    if (k_strcmp(c, "sysinfo") == 0) {
        char vendor[13];
        uint32_t ebx2, ecx2, edx2;
        __asm__ __volatile__("cpuid":"=b"(ebx2),"=c"(ecx2),"=d"(edx2):"a"(0));
        *((uint32_t*)&vendor[0])=ebx2;
        *((uint32_t*)&vendor[4])=edx2;
        *((uint32_t*)&vendor[8])=ecx2;
        vendor[12]='\0';
        gt_puts("  CPU    : "); gt_puts(vendor); gt_putc('\n');
        gt_puts("  Mimari : i686 32-bit\n");
        gt_puts("  Video  : VESA 800x600\n");
        char db[10];
        k_itoa(vesa_get_depth(), db, 10);
        gt_puts("  Renk   : "); gt_puts(db); gt_puts("-bit\n");
        return 0;
    }
    if (k_strncmp(c, "echo ", 5) == 0) {
        gt_puts("  "); gt_puts(c + 5); gt_putc('\n');
        return 0;
    }
    if (k_strcmp(c, "echo") == 0) { gt_putc('\n'); return 0; }
    if (k_strcmp(c, "reboot") == 0) { outb(0x64, 0xFE); return 0; }

    gt_puts("  Bilinmeyen: "); gt_puts(c); gt_putc('\n');
    gt_puts("  'help' yazin.\n");
    return 0;
}

static void gui_terminal(void) {
    gt_clear_buf();
    gt_puts("MyOS Terminal v0.3\n");
    gt_puts("Cikis icin 'exit' yazin.\n\n");
    gt_prompt();
    gt_render();

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            gt_cmd[gt_cmd_len] = '\0';
            int quit = gt_process_cmd();
            if (quit) return;
            gt_prompt();
        } else if (c == '\b') {
            if (gt_cmd_len > 0) {
                gt_cmd_len--;
                gt_cmd[gt_cmd_len] = '\0';
                gt_putc('\b');
            }
        } else if (c >= 32 && c < 127 && gt_cmd_len < 250) {
            gt_cmd[gt_cmd_len++] = c;
            gt_putc(c);
        }

        gt_render();
    }
}

/* ======== MASAUSTU ======== */
typedef struct { int x,y; const char* label; int id; } dicon_t;
static dicon_t dicons[] = {
    {30, 40,  "Bilgisayarim", 1},
    {30, 120, "Terminal",     2},
    {30, 200, "Hakkinda",     3},
    {30, 280, "Ekran Ayar",   4},
};
#define DICON_COUNT 4

static void draw_desktop_icons(void) {
    for (int i = 0; i < DICON_COUNT; i++) {
        int ix = dicons[i].x, iy = dicons[i].y;
        if (dicons[i].id == 1) draw_icon_computer(ix, iy);
        else if (dicons[i].id == 2) draw_icon_terminal(ix, iy);
        else if (dicons[i].id == 3) draw_icon_info(ix, iy);
        else draw_icon_display(ix, iy);
        int tw = k_strlen(dicons[i].label) * 8;
        vesa_draw_string_nobg(ix + 12 - tw/2, iy + 28, dicons[i].label, COLOR_TEXT_WHITE);
    }
}

static void draw_taskbar(void) {
    int sw = vesa_get_width(), sh = vesa_get_height();
    int ty = sh - 40;

    vesa_fill_rect(0, ty, sw, 40, COLOR_TASKBAR);
    vesa_fill_rect(0, ty, sw, 1, COLOR_WINDOW_BORDER);

    uint32_t sc = start_open ? COLOR_START_HOVER : COLOR_START_BTN;
    vesa_fill_rect(4, ty+4, 80, 32, sc);
    draw_windows_logo(12, ty+10, COLOR_TEXT_WHITE);
    vesa_draw_string(32, ty+12, "Baslat", COLOR_TEXT_WHITE, sc);

    char clock[6];
    get_time(clock);
    vesa_draw_string(sw-48, ty+12, clock, COLOR_TEXT_WHITE, COLOR_TASKBAR);

    draw_icon_gear(sw-75, ty+12, COLOR_TEXT_GREY);

    char dbuf[8];
    k_itoa(vesa_get_depth(), dbuf, 10);
    int dl = k_strlen(dbuf);
    dbuf[dl]='b'; dbuf[dl+1]='\0';
    vesa_draw_string(sw-115, ty+12, dbuf, COLOR_TEXT_CYAN, COLOR_TASKBAR);

    vesa_draw_string(sw/2-36, ty+12, "MyOS v0.3", COLOR_TEXT_GREY, COLOR_TASKBAR);
}

static void draw_start_menu(int mx, int my) {
    int sh = vesa_get_height();
    int smx = 0, smy = sh - 40 - 220;

    vesa_fill_rect(smx, smy, 240, 220, COLOR_MENU_BG);
    vesa_draw_rect_outline(smx, smy, 240, 220, COLOR_WINDOW_BORDER);

    vesa_fill_rect(smx+10, smy+10, 220, 30, COLOR_WINDOW_TITLE);
    draw_windows_logo(smx+18, smy+15, COLOR_ACCENT);
    vesa_draw_string(smx+46, smy+17, "MyOS User", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

    const char* items[] = {"Terminal","Sistem Bilgisi","Hakkinda","Yeniden Baslat","Kapat"};
    for (int i = 0; i < 5; i++) {
        int iy = smy + 50 + i * 32;
        int hov = (mx >= 0 && mx < 240 && my >= iy && my < iy + 32);
        uint32_t ibg = hov ? COLOR_MENU_HOVER : COLOR_MENU_BG;
        vesa_fill_rect(1, iy, 238, 32, ibg);
        vesa_draw_string(20, iy + 8, items[i], COLOR_TEXT_WHITE, ibg);
        if (i == 4) draw_icon_power(206, iy + 4);
    }
}

static void show_sysinfo_window(void) {
    int wx=180, wy=80, ww=440, wh=350;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Sistem Bilgisi", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24, wy+8, COLOR_CLOSE_BTN);

    char vendor[13];
    uint32_t ebx, ecx, edx;
    __asm__ __volatile__("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    *((uint32_t*)&vendor[0])=ebx;
    *((uint32_t*)&vendor[4])=edx;
    *((uint32_t*)&vendor[8])=ecx;
    vendor[12]='\0';

    int cy=wy+50;
    draw_icon_computer(wx+20, cy);
    vesa_draw_string(wx+60,cy+4,"CPU:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+100,cy+4,vendor,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);
    cy+=40;
    vesa_draw_string(wx+60,cy,"Mimari:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+130,cy,"i686 32-bit",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Video:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char res[20];
    k_itoa(vesa_get_width(),res,10);
    int rl=k_strlen(res);
    res[rl]='x'; k_itoa(vesa_get_height(),res+rl+1,10);
    vesa_draw_string(wx+130,cy,res,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Renk:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char db[10];
    k_itoa(vesa_get_depth(),db,10);
    int dl2=k_strlen(db);
    db[dl2]='-'; db[dl2+1]='b'; db[dl2+2]='i'; db[dl2+3]='t'; db[dl2+4]='\0';
    vesa_draw_string(wx+130,cy,db,COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Versiyon:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+150,cy,"MyOS v0.3",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
}

static void show_about_window(void) {
    int wx=200,wy=120,ww=400,wh=260;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Hakkinda",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    draw_windows_logo(wx+ww/2-8,wy+50,COLOR_ACCENT);
    vesa_draw_string(wx+ww/2-28,wy+80,"MyOS",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2-52,wy+100,"Versiyon 0.3.0",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+140,"Sifirdan yazilmis isletim sistemi",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+170,"x86 32-bit | VESA Grafik",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+200,"C + Assembly ile yazildi",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
}

/* ======== EKRAN AYARLARI ======== */
static const int dv[9] = {1, 4, 8, 16, 24, 30, 32, 36, 48};
static const int ds[9] = {1, 1, 1, 1,  1,  0,  1,  0,  0};

static void show_settings_window(int mx, int my) {
    int wx=140, wy=40, ww=520, wh=460;

    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran Ayarlari",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    /* Monitor kutusu */
    vesa_fill_rect(wx+20,wy+45,ww-40,70,0x1A1A2A);
    vesa_draw_rect_outline(wx+20,wy+45,ww-40,70,COLOR_WINDOW_BORDER);
    draw_icon_display(wx+30,wy+55);
    vesa_draw_string(wx+65,wy+55,"Monitor :",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+155,wy+55,"VBE Uyumlu",COLOR_TEXT_WHITE,0x1A1A2A);
    char res2[20];
    k_itoa(vesa_get_width(),res2,10);
    int rl=k_strlen(res2);
    res2[rl]='x'; k_itoa(vesa_get_height(),res2+rl+1,10);
    vesa_draw_string(wx+65,wy+75,"Cozunurluk:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+165,wy+75,res2,COLOR_TEXT_WHITE,0x1A1A2A);
    char hw[10];
    k_itoa(vesa_get_bpp(),hw,10);
    int hwl=k_strlen(hw);
    hw[hwl]='-'; hw[hwl+1]='b'; hw[hwl+2]='i'; hw[hwl+3]='t'; hw[hwl+4]='\0';
    vesa_draw_string(wx+65,wy+95,"Donanim  :",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+165,wy+95,hw,COLOR_TEXT_GREEN,0x1A1A2A);

    vesa_draw_string(wx+20,wy+125,"Renk Derinligi:",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);

    /* 9 buton */
    for (int i = 0; i < 9; i++) {
        int bx=wx+20+(i%3)*165, by=wy+145+(i/3)*50;
        int bw2=150, bh2=40;
        int hov = (ds[i] && mx>=bx && mx<bx+bw2 && my>=by && my<by+bh2);
        int sel = (dv[i] == settings_selected_depth);
        int act = (dv[i] == vesa_get_depth());

        uint32_t bg2, fg2;
        if (!ds[i])    { bg2=0x1A1A2A; fg2=0x555555; }
        else if (sel)  { bg2=COLOR_ACCENT; fg2=COLOR_TEXT_WHITE; }
        else if (hov)  { bg2=COLOR_BUTTON_HOVER; fg2=COLOR_TEXT_WHITE; }
        else           { bg2=COLOR_BUTTON; fg2=COLOR_TEXT_WHITE; }

        vesa_fill_rect(bx,by,bw2,bh2,bg2);
        vesa_draw_rect_outline(bx,by,bw2,bh2, sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if (act && ds[i])
            vesa_draw_rect_outline(bx+1,by+1,bw2-2,bh2-2,COLOR_TEXT_GREEN);

        char label[12];
        k_itoa(dv[i],label,10);
        int ll=k_strlen(label);
        label[ll]='-'; label[ll+1]='b'; label[ll+2]='i'; label[ll+3]='t'; label[ll+4]='\0';
        int tw2=k_strlen(label)*8;
        vesa_draw_string(bx+(bw2-tw2)/2, by+4, label, fg2, bg2);

        if (!ds[i]) {
            vesa_draw_string(bx+bw2/2-48, by+22, "desteklenmiyor", 0x773333, bg2);
        } else {
            const char* ci;
            switch(dv[i]) {
                case 1:  ci="2 renk"; break;
                case 4:  ci="64 renk"; break;
                case 8:  ci="256 renk"; break;
                case 16: ci="65K renk"; break;
                case 24: ci="16.7M renk"; break;
                case 32: ci="16.7M+Alpha"; break;
                default: ci=""; break;
            }
            int ciw=k_strlen(ci)*8;
            vesa_draw_string(bx+(bw2-ciw)/2, by+22, ci, COLOR_TEXT_GREY, bg2);
        }
    }

    /* Onizleme - SECILI derinlik ile gosterir (uygulamadan once) */
    int gy = wy + 310;
    vesa_draw_string(wx+20, gy, "Onizleme:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);

    /* Secili derinlik bilgisi */
    char seltxt[12];
    k_itoa(settings_selected_depth, seltxt, 10);
    int stl=k_strlen(seltxt);
    seltxt[stl]='-'; seltxt[stl+1]='b'; seltxt[stl+2]='i';
    seltxt[stl+3]='t'; seltxt[stl+4]='\0';
    vesa_draw_string(wx+100, gy, seltxt, COLOR_TEXT_YELLOW, COLOR_WINDOW_BG);

    gy += 20;
    int gw = ww - 40;

    /* Gradyan - secili derinlik ile render */
    for (int px = 0; px < gw; px++) {
        uint32_t r = (uint32_t)((px * 255) / gw);
        uint32_t g = 255 - r;
        uint32_t b = (uint32_t)((px * 128) / gw) + 64;
        uint32_t col = (r << 16) | (g << 8) | b;
        /* Secili derinlik ile onizleme */
        uint32_t prev_col = vesa_preview_color(col, settings_selected_depth);
        for (int py = 0; py < 16; py++)
            vesa_putpixel_raw(wx + 20 + px, gy + py, prev_col);
    }
    /* Renk kutuculari */
    int boxy = gy + 20;
    uint32_t test_colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
                               0xFF00FF, 0x00FFFF, 0x0078D4, 0xFFFFFF};
    for (int i = 0; i < 8; i++) {
        uint32_t tc = vesa_preview_color(test_colors[i], settings_selected_depth);
        int bxx = wx + 20 + i * (gw / 8);
        for (int py2 = 0; py2 < 16; py2++)
            for (int px2 = 0; px2 < gw/8 - 2; px2++)
                vesa_putpixel_raw(bxx + px2, boxy + py2, tc);
    }

    vesa_draw_rect_outline(wx+20, gy, gw, 16, COLOR_WINDOW_BORDER);
    vesa_draw_rect_outline(wx+20, boxy, gw, 16, COLOR_WINDOW_BORDER);

    /* Uygula butonu */
    int abx=wx+ww/2-80, aby=wy+wh-45;
    int ahov = (mx>=abx && mx<abx+160 && my>=aby && my<aby+35);
    uint32_t abg = ahov ? COLOR_START_HOVER : COLOR_ACCENT;
    vesa_fill_rect(abx, aby, 160, 35, abg);
    vesa_draw_rect_outline(abx, aby, 160, 35, 0x005599);
    vesa_draw_string(abx+44, aby+10, "Uygula", COLOR_TEXT_WHITE, abg);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);
    vesa_draw_string(320,290,"Kapatiliyor...",COLOR_TEXT_WHITE,COLOR_BLACK);
    vesa_copy_buffer();
    outb(0x604,0x2000); outb(0xB004,0x2000); outb(0x4004,0x3400);
    __asm__ __volatile__("cli; hlt");
}

void gui_run(void) {
    mouse_state_t ms;
    int sh = vesa_get_height(), sw = vesa_get_width();
    settings_selected_depth = vesa_get_depth();

    while (1) {
        mouse_poll(&ms);

        /* --- Her frame tamamen ciz --- */
        vesa_fill_screen(COLOR_BG);
        draw_desktop_icons();

        if (window_open == 1) show_sysinfo_window();
        if (window_open == 2) show_about_window();
        if (window_open == 3) show_settings_window(ms.x, ms.y);

        draw_taskbar();
        if (start_open) draw_start_menu(ms.x, ms.y);

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        /* --- Tiklama --- */
        if (!ms.click) continue;

        /* Pencere kapat butonlari */
        if (window_open == 1) {
            if (ms.x>=180+440-24 && ms.x<180+440-8 && ms.y>=80+8 && ms.y<80+24)
                { window_open=0; continue; }
        }
        if (window_open == 2) {
            if (ms.x>=200+400-24 && ms.x<200+400-8 && ms.y>=120+8 && ms.y<120+24)
                { window_open=0; continue; }
        }
        if (window_open == 3) {
            int wx3=140, wy3=40, ww3=520, wh3=460;
            if (ms.x>=wx3+ww3-24 && ms.x<wx3+ww3-8 && ms.y>=wy3+8 && ms.y<wy3+24)
                { window_open=0; continue; }

            for (int i = 0; i < 9; i++) {
                int bx=wx3+20+(i%3)*165, by=wy3+145+(i/3)*50;
                if (ds[i] && ms.x>=bx && ms.x<bx+150 && ms.y>=by && ms.y<by+40) {
                    settings_selected_depth = dv[i];
                    break;
                }
            }
            int abx=wx3+ww3/2-80, aby=wy3+wh3-45;
            if (ms.x>=abx && ms.x<abx+160 && ms.y>=aby && ms.y<aby+35) {
                vesa_set_depth(settings_selected_depth);
                window_open=0;
            }
            continue;
        }

        /* Baslat */
        if (ms.x>=4 && ms.x<84 && ms.y>=sh-36 && ms.y<sh-4) {
            start_open = !start_open; window_open=0; continue;
        }

        /* Gear ikonu */
        if (ms.x>=sw-85 && ms.x<sw-55 && ms.y>=sh-32 && ms.y<sh-8) {
            window_open=3; settings_selected_depth=vesa_get_depth(); start_open=0; continue;
        }

        /* Baslat menu */
        if (start_open) {
            int smy = sh-40-220;
            for (int i = 0; i < 5; i++) {
                int iy = smy+50+i*32;
                if (ms.x>=0 && ms.x<240 && ms.y>=iy && ms.y<iy+32) {
                    start_open=0;
                    if (i==0) { gui_terminal(); }
                    else if (i==1) { window_open=1; }
                    else if (i==2) { window_open=2; }
                    else if (i==3) { outb(0x64,0xFE); }
                    else if (i==4) { do_shutdown(); }
                    break;
                }
            }
            if (ms.x>240 || ms.y<(sh-40-220)) start_open=0;
            continue;
        }

        /* Masaustu ikonlari */
        for (int i = 0; i < DICON_COUNT; i++) {
            int ix=dicons[i].x, iy=dicons[i].y;
            if (ms.x>=ix && ms.x<ix+60 && ms.y>=iy && ms.y<iy+50) {
                if (dicons[i].id==1) { window_open=1; }
                else if (dicons[i].id==2) { gui_terminal(); }
                else if (dicons[i].id==3) { window_open=2; }
                else if (dicons[i].id==4) { window_open=3; settings_selected_depth=vesa_get_depth(); }
                start_open=0; break;
            }
        }
    }
}
