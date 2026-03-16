#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"
#include "shell.h"

static int start_open = 0;
static int window_open = 0; /* 0=yok 1=sysinfo 2=about 3=settings */
static int settings_selected_depth = 32;

static uint8_t bcd2bin(uint8_t b) { return ((b>>4)*10)+(b&0x0F); }

static void get_time(char* buf) {
    outb(0x70,0x04); uint8_t h=bcd2bin(inb(0x71));
    outb(0x70,0x02); uint8_t m=bcd2bin(inb(0x71));
    buf[0]='0'+h/10; buf[1]='0'+h%10; buf[2]=':';
    buf[3]='0'+m/10; buf[4]='0'+m%10; buf[5]='\0';
}

/* Renk derinligi bilgisi */
static const char* depth_color_info(int d) {
    switch(d) {
        case 1:  return "2 renk (Siyah-Beyaz)";
        case 4:  return "64 renk (~16 ton)";
        case 8:  return "256 renk (Klasik)";
        case 16: return "65,536 renk (High Color)";
        case 24: return "16,777,216 renk (True Color)";
        case 30: return "Donanim desteklemiyor";
        case 32: return "16,777,216 + Alpha";
        case 36: return "Donanim desteklemiyor";
        case 48: return "Donanim desteklemiyor";
        default: return "";
    }
}

/* Masaustu ikonlari */
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
        int ix=dicons[i].x, iy=dicons[i].y;
        if (dicons[i].id == 1) draw_icon_computer(ix, iy);
        else if (dicons[i].id == 2) draw_icon_terminal(ix, iy);
        else if (dicons[i].id == 3) draw_icon_info(ix, iy);
        else if (dicons[i].id == 4) draw_icon_display(ix, iy);
        int tw = k_strlen(dicons[i].label) * 8;
        vesa_draw_string_nobg(ix + 12 - tw/2, iy+28, dicons[i].label, COLOR_TEXT_WHITE);
    }
}

static void draw_taskbar(void) {
    int sw = vesa_get_width();
    int sh = vesa_get_height();
    int th = 40, ty = sh - th;

    vesa_fill_rect(0, ty, sw, th, COLOR_TASKBAR);
    vesa_fill_rect(0, ty, sw, 1, COLOR_WINDOW_BORDER);

    /* Baslat butonu */
    uint32_t sc = start_open ? COLOR_START_HOVER : COLOR_START_BTN;
    vesa_fill_rect(4, ty+4, 80, 32, sc);
    draw_windows_logo(12, ty+10, COLOR_TEXT_WHITE);
    vesa_draw_string(32, ty+12, "Baslat", COLOR_TEXT_WHITE, sc);

    /* Saat */
    char clock[6];
    get_time(clock);
    vesa_draw_string(sw-48, ty+12, clock, COLOR_TEXT_WHITE, COLOR_TASKBAR);

    /* Gear ikonu (saatten once) */
    draw_icon_gear(sw-75, ty+12, COLOR_TEXT_GREY);

    /* Aktif derinlik gostergesi */
    char dbuf[8];
    k_itoa(vesa_get_depth(), dbuf, 10);
    int dl = k_strlen(dbuf);
    dbuf[dl]='b'; dbuf[dl+1]='\0';
    vesa_draw_string(sw-115, ty+12, dbuf, COLOR_TEXT_CYAN, COLOR_TASKBAR);

    /* Ortada MyOS */
    vesa_draw_string(sw/2-36, ty+12, "MyOS v0.3", COLOR_TEXT_GREY, COLOR_TASKBAR);
}

static void draw_start_menu(void) {
    int sh = vesa_get_height();
    int mx2=0, my2=sh-40-220, mw=240, mh=220;
    vesa_fill_rect(mx2, my2, mw, mh, COLOR_MENU_BG);
    vesa_draw_rect_outline(mx2, my2, mw, mh, COLOR_WINDOW_BORDER);

    vesa_fill_rect(mx2+10, my2+10, mw-20, 30, COLOR_WINDOW_TITLE);
    draw_windows_logo(mx2+18, my2+15, COLOR_ACCENT);
    vesa_draw_string(mx2+46, my2+17, "MyOS User", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

    const char* items[] = {"Terminal","Sistem Bilgisi","Hakkinda","Yeniden Baslat","Kapat"};
    for (int i = 0; i < 5; i++) {
        int iy = my2 + 50 + i * 32;
        vesa_draw_string(mx2+20, iy+8, items[i], COLOR_TEXT_WHITE, COLOR_MENU_BG);
        if (i == 4) draw_icon_power(mx2+mw-34, iy+4);
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
    vesa_draw_string(wx+60, cy+4, "CPU:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+100,cy+4, vendor, COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
    cy+=40;
    vesa_draw_string(wx+60,cy,"Mimari:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+130,cy,"i686 32-bit", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Video:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    char res[20];
    k_itoa(vesa_get_width(), res, 10);
    int l2 = k_strlen(res);
    res[l2]='x'; k_itoa(vesa_get_height(), res+l2+1, 10);
    vesa_draw_string(wx+130,cy,res, COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Derinlik:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    char db[10];
    k_itoa(vesa_get_depth(), db, 10);
    int dl2 = k_strlen(db);
    db[dl2]='-'; db[dl2+1]='b'; db[dl2+2]='i'; db[dl2+3]='t'; db[dl2+4]='\0';
    vesa_draw_string(wx+150,cy,db, COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Versiyon:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+150,cy,"MyOS v0.3", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
}

static void show_about_window(void) {
    int wx=200, wy=120, ww=400, wh=260;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Hakkinda", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24, wy+8, COLOR_CLOSE_BTN);

    draw_windows_logo(wx+ww/2-8, wy+50, COLOR_ACCENT);
    vesa_draw_string(wx+ww/2-28, wy+80, "MyOS", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2-52, wy+100,"Versiyon 0.3.0", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+40, wy+140, "Sifirdan yazilmis isletim sistemi", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    vesa_draw_string(wx+40, wy+170, "x86 32-bit | VESA Grafik", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+40, wy+200, "C + Assembly ile yazildi", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
}

/* ====== EKRAN AYARLARI PENCERESI ====== */
static const int depth_values[9] = {1, 4, 8, 16, 24, 30, 32, 36, 48};
static const int depth_supported[9] = {1, 1, 1, 1, 1, 0, 1, 0, 0};

static void show_settings_window(int mouse_x, int mouse_y) {
    int wx=140, wy=40, ww=520, wh=460;

    /* Pencere */
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8, wy+8, COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30, wy+8, "Ekran Ayarlari", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24, wy+8, COLOR_CLOSE_BTN);

    /* Monitor Bilgisi Kutusu */
    vesa_fill_rect(wx+20, wy+45, ww-40, 85, 0x1A1A2A);
    vesa_draw_rect_outline(wx+20, wy+45, ww-40, 85, COLOR_WINDOW_BORDER);

    draw_icon_display(wx+30, wy+55);

    vesa_draw_string(wx+65, wy+55, "Monitor :", COLOR_TEXT_GREY, 0x1A1A2A);
    vesa_draw_string(wx+155,wy+55, "VBE Uyumlu Monitor", COLOR_TEXT_WHITE, 0x1A1A2A);

    vesa_draw_string(wx+65, wy+75, "Cozunurluk:", COLOR_TEXT_GREY, 0x1A1A2A);
    char res2[20];
    k_itoa(vesa_get_width(), res2, 10);
    int rl = k_strlen(res2);
    res2[rl]='x'; k_itoa(vesa_get_height(), res2+rl+1, 10);
    int rl2 = k_strlen(res2);
    res2[rl2]=' '; res2[rl2+1]='p'; res2[rl2+2]='x'; res2[rl2+3]='\0';
    vesa_draw_string(wx+155,wy+75, res2, COLOR_TEXT_WHITE, 0x1A1A2A);

    vesa_draw_string(wx+65, wy+95, "Donanim :", COLOR_TEXT_GREY, 0x1A1A2A);
    char hw[20];
    k_itoa(vesa_get_bpp(), hw, 10);
    int hwl = k_strlen(hw);
    hw[hwl]='-'; hw[hwl+1]='b'; hw[hwl+2]='i'; hw[hwl+3]='t';
    hw[hwl+4]=' '; hw[hwl+5]='m'; hw[hwl+6]='a'; hw[hwl+7]='x';
    hw[hwl+8]='\0';
    vesa_draw_string(wx+155,wy+95, hw, COLOR_TEXT_GREEN, 0x1A1A2A);

    /* Baslik */
    vesa_draw_string(wx+20, wy+145, "Renk Derinligi Secin:", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);

    /* 9 buton (3x3 grid) */
    for (int i = 0; i < 9; i++) {
        int bx = wx + 20 + (i % 3) * 165;
        int by = wy + 170 + (i / 3) * 50;
        int bw = 150, bh = 40;
        int sup = depth_supported[i];
        int sel = (depth_values[i] == settings_selected_depth);
        int active = (depth_values[i] == vesa_get_depth());
        int hover = (sup && mouse_x >= bx && mouse_x < bx+bw &&
                     mouse_y >= by && mouse_y < by+bh);

        /* Buton rengi */
        uint32_t bg, fg;
        if (!sup) {
            bg = 0x1A1A2A; fg = 0x555555;
        } else if (sel) {
            bg = COLOR_ACCENT; fg = COLOR_TEXT_WHITE;
        } else if (hover) {
            bg = COLOR_BUTTON_HOVER; fg = COLOR_TEXT_WHITE;
        } else {
            bg = COLOR_BUTTON; fg = COLOR_TEXT_WHITE;
        }

        vesa_fill_rect(bx, by, bw, bh, bg);
        vesa_draw_rect_outline(bx, by, bw, bh, sel ? 0x44AAFF : COLOR_WINDOW_BORDER);

        /* Aktif derinlik - yesil kenar */
        if (active && sup) {
            vesa_draw_rect_outline(bx+1, by+1, bw-2, bh-2, COLOR_TEXT_GREEN);
        }

        /* Buton yazisi */
        char label[12];
        k_itoa(depth_values[i], label, 10);
        int ll = k_strlen(label);
        label[ll]='-'; label[ll+1]='b'; label[ll+2]='i';
        label[ll+3]='t'; label[ll+4]='\0';

        int tw2 = k_strlen(label) * 8;
        vesa_draw_string(bx + (bw-tw2)/2, by+4, label, fg, bg);

        /* Desteklenmiyor isareti */
        if (!sup) {
            vesa_draw_string(bx + bw/2 - 48, by+22, "desteklenmiyor", 0x773333, bg);
        } else {
            /* Renk sayisi */
            const char* cinfo;
            switch(depth_values[i]) {
                case 1:  cinfo = "2 renk"; break;
                case 4:  cinfo = "64 renk"; break;
                case 8:  cinfo = "256 renk"; break;
                case 16: cinfo = "65K renk"; break;
                case 24: cinfo = "16.7M renk"; break;
                case 32: cinfo = "16.7M+Alpha"; break;
                default: cinfo = ""; break;
            }
            int ciw = k_strlen(cinfo) * 8;
            vesa_draw_string(bx + (bw-ciw)/2, by+22, cinfo, COLOR_TEXT_GREY, bg);
        }
    }

    /* Secili derinlik bilgisi */
    int iy = wy + 325;
    vesa_draw_string(wx+20, iy, "Secili:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    char seltxt[12];
    k_itoa(settings_selected_depth, seltxt, 10);
    int stl = k_strlen(seltxt);
    seltxt[stl]='-'; seltxt[stl+1]='b'; seltxt[stl+2]='i';
    seltxt[stl+3]='t'; seltxt[stl+4]='\0';
    vesa_draw_string(wx+80, iy, seltxt, COLOR_TEXT_YELLOW, COLOR_WINDOW_BG);

    const char* dinfo = depth_color_info(settings_selected_depth);
    vesa_draw_string(wx+20, iy+20, dinfo, COLOR_TEXT_WHITE, COLOR_WINDOW_BG);

    /* Renk onizleme gradyani */
    int gy = wy + 370;
    vesa_draw_string(wx+20, gy, "Onizleme:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    gy += 20;
    int gw = ww - 40;
    for (int px = 0; px < gw; px++) {
        uint32_t r = (uint32_t)((px * 255) / gw);
        uint32_t g = 255 - r;
        uint32_t b = (uint32_t)((px * 128) / gw) + 64;
        uint32_t col = (r << 16) | (g << 8) | b;
        for (int py = 0; py < 20; py++)
            vesa_putpixel(wx + 20 + px, gy + py, col);
    }
    vesa_draw_rect_outline(wx+20, gy, gw, 20, COLOR_WINDOW_BORDER);

    /* Uygula butonu */
    int abx = wx + ww/2 - 80, aby = wy + 420;
    int ahover = (mouse_x >= abx && mouse_x < abx+160 &&
                  mouse_y >= aby && mouse_y < aby+35);
    uint32_t abg = ahover ? COLOR_START_HOVER : COLOR_ACCENT;
    vesa_fill_rect(abx, aby, 160, 35, abg);
    vesa_draw_rect_outline(abx, aby, 160, 35, 0x005599);
    vesa_draw_string(abx+40, aby+10, "Uygula", COLOR_TEXT_WHITE, abg);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);
    vesa_draw_string(320, 290, "Kapatiliyor...", COLOR_TEXT_WHITE, COLOR_BLACK);
    vesa_copy_buffer();
    outb(0x604, 0x2000);
    outb(0xB004, 0x2000);
    outb(0x4004, 0x3400);
    __asm__ __volatile__("cli; hlt");
}

void gui_run(void) {
    mouse_state_t ms;
    int sw = vesa_get_width();
    int sh = vesa_get_height();
    settings_selected_depth = vesa_get_depth();

    while (1) {
        mouse_poll(&ms);

        /* --- Cizim --- */
        vesa_fill_screen(COLOR_BG);
        draw_desktop_icons();

        if (window_open == 1) show_sysinfo_window();
        if (window_open == 2) show_about_window();
        if (window_open == 3) show_settings_window(ms.x, ms.y);

        draw_taskbar();
        if (start_open) draw_start_menu();

        /* Start menu hover */
        if (start_open) {
            int my2 = sh - 40 - 220;
            for (int i = 0; i < 5; i++) {
                int iy = my2 + 50 + i * 32;
                if (ms.x >= 0 && ms.x < 240 && ms.y >= iy && ms.y < iy+32)
                    vesa_fill_rect(1, iy, 238, 32, COLOR_MENU_HOVER);
            }
            /* Menu yazilarini tekrar ciz (hover uzerine) */
            const char* items2[] = {"Terminal","Sistem Bilgisi","Hakkinda","Yeniden Baslat","Kapat"};
            for (int i = 0; i < 5; i++) {
                int iy = my2 + 50 + i * 32;
                int hov = (ms.x >= 0 && ms.x < 240 && ms.y >= iy && ms.y < iy+32);
                uint32_t ibg = hov ? COLOR_MENU_HOVER : COLOR_MENU_BG;
                vesa_draw_string(20, iy+8, items2[i], COLOR_TEXT_WHITE, ibg);
                if (i == 4) draw_icon_power(206, iy+4);
            }
        }

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        /* --- Tiklama --- */
        if (!ms.click) continue;

        /* Pencere kapat butonlari */
        if (window_open == 1) {
            int wx3=180,wy3=80,ww3=440;
            if (ms.x>=wx3+ww3-24 && ms.x<wx3+ww3-8 && ms.y>=wy3+8 && ms.y<wy3+24)
                { window_open=0; continue; }
        }
        if (window_open == 2) {
            int wx3=200,wy3=120,ww3=400;
            if (ms.x>=wx3+ww3-24 && ms.x<wx3+ww3-8 && ms.y>=wy3+8 && ms.y<wy3+24)
                { window_open=0; continue; }
        }
        if (window_open == 3) {
            int wx3=140,wy3=40,ww3=520;

            /* Kapat butonu */
            if (ms.x>=wx3+ww3-24 && ms.x<wx3+ww3-8 && ms.y>=wy3+8 && ms.y<wy3+24)
                { window_open=0; continue; }

            /* Derinlik butonlari */
            for (int i = 0; i < 9; i++) {
                int bx=wx3+20+(i%3)*165;
                int by=wy3+170+(i/3)*50;
                if (depth_supported[i] &&
                    ms.x>=bx && ms.x<bx+150 && ms.y>=by && ms.y<by+40) {
                    settings_selected_depth = depth_values[i];
                    break;
                }
            }

            /* Uygula butonu */
            int abx=wx3+ww3/2-80, aby=wy3+420;
            if (ms.x>=abx && ms.x<abx+160 && ms.y>=aby && ms.y<aby+35) {
                vesa_set_depth(settings_selected_depth);
                window_open = 0;
            }
            continue;
        }

        /* Baslat butonu */
        if (ms.x>=4 && ms.x<84 && ms.y>=sh-36 && ms.y<sh-4) {
            start_open = !start_open;
            window_open = 0;
            continue;
        }

        /* Gear ikonu (taskbar) */
        if (ms.x>=sw-85 && ms.x<sw-55 && ms.y>=sh-32 && ms.y<sh-8) {
            window_open = 3;
            settings_selected_depth = vesa_get_depth();
            start_open = 0;
            continue;
        }

        /* Start menu tiklama */
        if (start_open) {
            int my2 = sh-40-220;
            for (int i = 0; i < 5; i++) {
                int iy = my2+50+i*32;
                if (ms.x>=0 && ms.x<240 && ms.y>=iy && ms.y<iy+32) {
                    start_open = 0;
                    if (i==1) { window_open=1; }
                    else if (i==2) { window_open=2; }
                    else if (i==3) { outb(0x64, 0xFE); }
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
                if (dicons[i].id==1) window_open=1;
                else if (dicons[i].id==3) window_open=2;
                else if (dicons[i].id==4) {
                    window_open=3;
                    settings_selected_depth=vesa_get_depth();
                }
                start_open=0;
                break;
            }
        }
    }
}
