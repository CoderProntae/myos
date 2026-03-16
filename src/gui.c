#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"

static int start_open = 0;
/* 0=yok 1=sysinfo 2=about 3=renk ayar 4=hz ayar 5=hata popup */
static int window_open = 0;
static int settings_selected_depth = 32;
static int current_hz = 60;
static int selected_hz = 60;
static int error_timer = 0;

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

static void gt_clear_buf(void) {
    for (int r = 0; r < GT_ROWS; r++)
        k_memset(gt_buf[r], 0, GT_COLS + 1);
    gt_cx = 0; gt_cy = 0;
}

static void gt_scroll(void) {
    while (gt_cy >= GT_ROWS) {
        for (int r = 0; r < GT_ROWS - 1; r++)
            for (int c = 0; c <= GT_COLS; c++)
                gt_buf[r][c] = gt_buf[r + 1][c];
        k_memset(gt_buf[GT_ROWS - 1], 0, GT_COLS + 1);
        gt_cy--;
    }
}

static void gt_putc(char c) {
    if (c == '\n') { gt_cx = 0; gt_cy++; gt_scroll(); }
    else if (c == '\b') { if (gt_cx > 0) { gt_cx--; gt_buf[gt_cy][gt_cx] = ' '; } }
    else if (gt_cx < GT_COLS) {
        gt_buf[gt_cy][gt_cx] = c;
        gt_cx++;
        if (gt_cx >= GT_COLS) { gt_cx = 0; gt_cy++; gt_scroll(); }
    }
}

static void gt_puts(const char* s) { while (*s) gt_putc(*s++); }

static void gt_render(void) {
    int sw = vesa_get_width(), sh = vesa_get_height();
    int wx = 20, wy = 10, ww = sw - 40, wh = sh - 50;

    vesa_fill_screen(COLOR_BG);
    vesa_fill_rect(wx, wy, ww, wh, COLOR_TERMINAL_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);

    vesa_fill_rect(wx+8, wy+6, 20, 20, 0x333355);
    vesa_fill_rect(wx+9, wy+7, 18, 18, COLOR_TERMINAL_BG);
    vesa_fill_rect(wx+12, wy+12, 2, 2, COLOR_TEXT_GREEN);
    vesa_fill_rect(wx+14, wy+14, 2, 2, COLOR_TEXT_GREEN);
    vesa_fill_rect(wx+12, wy+16, 2, 2, COLOR_TEXT_GREEN);
    vesa_fill_rect(wx+17, wy+14, 5, 2, COLOR_TEXT_GREY);
    vesa_draw_string(wx+36, wy+8, "Terminal - MyOS", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

    vesa_fill_rect(wx+ww-28, wy+4, 24, 24, COLOR_CLOSE_BTN);
    for (int i = 0; i < 10; i++) {
        vesa_putpixel(wx+ww-22+i, wy+10+i, COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-13-i, wy+10+i, COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-21+i, wy+10+i, COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-14-i, wy+10+i, COLOR_TEXT_WHITE);
    }

    for (int r = 0; r < GT_ROWS; r++)
        for (int c = 0; c < GT_COLS; c++) {
            char ch = gt_buf[r][c];
            if (ch && ch != ' ') {
                uint32_t fg = COLOR_TEXT_GREY;
                if (c < 8) fg = COLOR_TEXT_GREEN;
                vesa_draw_char(GT_OX + c*8, GT_OY + r*16, ch, fg, COLOR_TERMINAL_BG);
            }
        }

    vesa_fill_rect(GT_OX + gt_cx*8, GT_OY + gt_cy*16, 8, 16, 0x888888);
    vesa_copy_buffer();
}

static void gt_prompt(void) {
    gt_puts("myos:~$ ");
    gt_cmd_len = 0;
    k_memset(gt_cmd, 0, 256);
}

static int gt_process_cmd(void) {
    gt_putc('\n');
    const char* c = gt_cmd;
    while (*c == ' ') c++;
    if (k_strlen(c) == 0) return 0;
    if (k_strcmp(c, "exit") == 0) return 1;
    if (k_strcmp(c, "clear") == 0) { gt_clear_buf(); return 0; }
    if (k_strcmp(c, "help") == 0) {
        gt_puts("  help clear echo time sysinfo exit reboot\n");
        return 0;
    }
    if (k_strcmp(c, "time") == 0) {
        outb(0x70,0x04); uint8_t h=bcd2bin(inb(0x71));
        outb(0x70,0x02); uint8_t m=bcd2bin(inb(0x71));
        outb(0x70,0x00); uint8_t s=bcd2bin(inb(0x71));
        char tb[12];
        tb[0]=' '; tb[1]=' ';
        tb[2]='0'+h/10; tb[3]='0'+h%10; tb[4]=':';
        tb[5]='0'+m/10; tb[6]='0'+m%10; tb[7]=':';
        tb[8]='0'+s/10; tb[9]='0'+s%10; tb[10]='\n'; tb[11]='\0';
        gt_puts(tb); return 0;
    }
    if (k_strcmp(c, "sysinfo") == 0) {
        char vendor[13];
        uint32_t ebx2,ecx2,edx2;
        __asm__ __volatile__("cpuid":"=b"(ebx2),"=c"(ecx2),"=d"(edx2):"a"(0));
        *((uint32_t*)&vendor[0])=ebx2;
        *((uint32_t*)&vendor[4])=edx2;
        *((uint32_t*)&vendor[8])=ecx2;
        vendor[12]='\0';
        gt_puts("  CPU   : "); gt_puts(vendor); gt_putc('\n');
        gt_puts("  Video : VESA 800x600\n");
        char db[10]; k_itoa(vesa_get_depth(),db,10);
        gt_puts("  Renk  : "); gt_puts(db); gt_puts("-bit\n");
        char hb[10]; k_itoa(current_hz,hb,10);
        gt_puts("  Hz    : "); gt_puts(hb); gt_puts(" Hz\n");
        return 0;
    }
    if (k_strncmp(c,"echo ",5)==0) { gt_puts("  "); gt_puts(c+5); gt_putc('\n'); return 0; }
    if (k_strcmp(c,"reboot")==0) { outb(0x64,0xFE); return 0; }
    gt_puts("  Bilinmeyen: "); gt_puts(c); gt_puts("\n  'help' yazin.\n");
    return 0;
}

static void gui_terminal(void) {
    gt_clear_buf();
    gt_puts("MyOS Terminal v0.3\n");
    gt_puts("Cikis icin 'exit' yazin.\n\n");
    gt_prompt(); gt_render();
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            gt_cmd[gt_cmd_len] = '\0';
            if (gt_process_cmd()) return;
            gt_prompt();
        } else if (c == '\b') {
            if (gt_cmd_len > 0) { gt_cmd_len--; gt_cmd[gt_cmd_len]='\0'; gt_putc('\b'); }
        } else if (c >= 32 && c < 127 && gt_cmd_len < 250) {
            gt_cmd[gt_cmd_len++] = c; gt_putc(c);
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
        int ix=dicons[i].x, iy=dicons[i].y;
        if (dicons[i].id==1) draw_icon_computer(ix,iy);
        else if (dicons[i].id==2) draw_icon_terminal(ix,iy);
        else if (dicons[i].id==3) draw_icon_info(ix,iy);
        else draw_icon_display(ix,iy);
        int tw=k_strlen(dicons[i].label)*8;
        vesa_draw_string_nobg(ix+12-tw/2, iy+28, dicons[i].label, COLOR_TEXT_WHITE);
    }
}

static void draw_taskbar(void) {
    int sw=vesa_get_width(), sh=vesa_get_height(), ty=sh-40;
    vesa_fill_rect(0,ty,sw,40,COLOR_TASKBAR);
    vesa_fill_rect(0,ty,sw,1,COLOR_WINDOW_BORDER);

    uint32_t sc = start_open ? COLOR_START_HOVER : COLOR_START_BTN;
    vesa_fill_rect(4,ty+4,80,32,sc);
    draw_windows_logo(12,ty+10,COLOR_TEXT_WHITE);
    vesa_draw_string(32,ty+12,"Baslat",COLOR_TEXT_WHITE,sc);

    char clock[6]; get_time(clock);
    vesa_draw_string(sw-48,ty+12,clock,COLOR_TEXT_WHITE,COLOR_TASKBAR);
    draw_icon_gear(sw-75,ty+12,COLOR_TEXT_GREY);

    /* Derinlik + Hz gostergesi */
    char dbuf[16];
    k_itoa(vesa_get_depth(),dbuf,10);
    int dl=k_strlen(dbuf);
    dbuf[dl]='b'; dbuf[dl+1]=' '; dbuf[dl+2]='\0';
    int dl2=k_strlen(dbuf);
    k_itoa(current_hz,dbuf+dl2,10);
    int dl3=k_strlen(dbuf);
    dbuf[dl3]='H'; dbuf[dl3+1]='z'; dbuf[dl3+2]='\0';
    vesa_draw_string(sw-145,ty+12,dbuf,COLOR_TEXT_CYAN,COLOR_TASKBAR);

    vesa_draw_string(sw/2-36,ty+12,"MyOS v0.3",COLOR_TEXT_GREY,COLOR_TASKBAR);
}

static void draw_start_menu(int mx, int my) {
    int sh=vesa_get_height(), smy=sh-40-220;
    vesa_fill_rect(0,smy,240,220,COLOR_MENU_BG);
    vesa_draw_rect_outline(0,smy,240,220,COLOR_WINDOW_BORDER);
    vesa_fill_rect(10,smy+10,220,30,COLOR_WINDOW_TITLE);
    draw_windows_logo(18,smy+15,COLOR_ACCENT);
    vesa_draw_string(46,smy+17,"MyOS User",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);

    const char* items[]={"Terminal","Sistem Bilgisi","Hakkinda","Yeniden Baslat","Kapat"};
    for (int i = 0; i < 5; i++) {
        int iy=smy+50+i*32;
        int hov=(mx>=0 && mx<240 && my>=iy && my<iy+32);
        uint32_t ibg = hov ? COLOR_MENU_HOVER : COLOR_MENU_BG;
        vesa_fill_rect(1,iy,238,32,ibg);
        vesa_draw_string(20,iy+8,items[i],COLOR_TEXT_WHITE,ibg);
        if (i==4) draw_icon_power(206,iy+4);
    }
}

static void show_sysinfo_window(void) {
    int wx=160,wy=60,ww=480,wh=400;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Sistem Bilgisi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    char vendor[13];
    uint32_t ebx,ecx,edx;
    __asm__ __volatile__("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    *((uint32_t*)&vendor[0])=ebx;
    *((uint32_t*)&vendor[4])=edx;
    *((uint32_t*)&vendor[8])=ecx;
    vendor[12]='\0';

    int cy=wy+50;
    draw_icon_computer(wx+20,cy);
    vesa_draw_string(wx+60,cy+4,"CPU:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+110,cy+4,vendor,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);
    cy+=40;
    vesa_draw_string(wx+60,cy,"Mimari:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+140,cy,"i686 32-bit",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Video:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char res[20];
    k_itoa(vesa_get_width(),res,10);
    int rl=k_strlen(res);
    res[rl]='x'; k_itoa(vesa_get_height(),res+rl+1,10);
    vesa_draw_string(wx+140,cy,res,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Renk:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char db[10]; k_itoa(vesa_get_depth(),db,10);
    int dl2=k_strlen(db);
    db[dl2]='-'; db[dl2+1]='b'; db[dl2+2]='i'; db[dl2+3]='t'; db[dl2+4]='\0';
    vesa_draw_string(wx+140,cy,db,COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Yenileme:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char hb[10]; k_itoa(current_hz,hb,10);
    int hl=k_strlen(hb);
    hb[hl]=' '; hb[hl+1]='H'; hb[hl+2]='z'; hb[hl+3]='\0';
    vesa_draw_string(wx+140,cy,hb,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Versiyon:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+140,cy,"MyOS v0.3",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
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

/* ======== RENK DERINLIGI AYARLARI ======== */
static const int dv[9] = {1,4,8,16,24,30,32,36,48};
static const int ds[9] = {1,1,1,1, 1, 0, 1, 0, 0};

static void show_settings_window(int mx, int my) {
    int wx=140,wy=40,ww=520,wh=480;

    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran Ayarlari - Renk Derinligi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    /* Tab butonlari */
    vesa_fill_rect(wx+10,wy+38,160,28,COLOR_ACCENT);
    vesa_draw_string(wx+22,wy+44,"Renk Derinligi",COLOR_TEXT_WHITE,COLOR_ACCENT);
    int hz_tab_hover = (mx>=wx+180 && mx<wx+340 && my>=wy+38 && my<wy+66);
    uint32_t hz_tab_bg = hz_tab_hover ? COLOR_BUTTON_HOVER : COLOR_BUTTON;
    vesa_fill_rect(wx+180,wy+38,160,28,hz_tab_bg);
    vesa_draw_string(wx+196,wy+44,"Yenileme Hizi >>",COLOR_TEXT_WHITE,hz_tab_bg);

    /* Monitor kutusu */
    vesa_fill_rect(wx+20,wy+75,ww-40,55,0x1A1A2A);
    vesa_draw_rect_outline(wx+20,wy+75,ww-40,55,COLOR_WINDOW_BORDER);
    draw_icon_display(wx+30,wy+82);
    vesa_draw_string(wx+65,wy+82,"Monitor:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+155,wy+82,"VBE Uyumlu",COLOR_TEXT_WHITE,0x1A1A2A);
    char hw[10]; k_itoa(vesa_get_bpp(),hw,10);
    int hwl=k_strlen(hw);
    hw[hwl]='-'; hw[hwl+1]='b'; hw[hwl+2]='i'; hw[hwl+3]='t'; hw[hwl+4]='\0';
    vesa_draw_string(wx+65,wy+100,"Donanim:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+155,wy+100,hw,COLOR_TEXT_GREEN,0x1A1A2A);

    vesa_draw_string(wx+20,wy+140,"Renk Derinligi Secin:",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);

    /* 9 buton */
    for (int i = 0; i < 9; i++) {
        int bx=wx+20+(i%3)*165, by=wy+160+(i/3)*50;
        int bw2=150,bh2=40;
        int hov=(ds[i]&&mx>=bx&&mx<bx+bw2&&my>=by&&my<by+bh2);
        int sel=(dv[i]==settings_selected_depth);
        int act=(dv[i]==vesa_get_depth());
        uint32_t bg2,fg2;
        if (!ds[i])   { bg2=0x1A1A2A; fg2=0x555555; }
        else if (sel) { bg2=COLOR_ACCENT; fg2=COLOR_TEXT_WHITE; }
        else if (hov) { bg2=COLOR_BUTTON_HOVER; fg2=COLOR_TEXT_WHITE; }
        else          { bg2=COLOR_BUTTON; fg2=COLOR_TEXT_WHITE; }
        vesa_fill_rect(bx,by,bw2,bh2,bg2);
        vesa_draw_rect_outline(bx,by,bw2,bh2,sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if (act&&ds[i]) vesa_draw_rect_outline(bx+1,by+1,bw2-2,bh2-2,COLOR_TEXT_GREEN);

        char label[12]; k_itoa(dv[i],label,10);
        int ll=k_strlen(label);
        label[ll]='-'; label[ll+1]='b'; label[ll+2]='i'; label[ll+3]='t'; label[ll+4]='\0';
        int tw2=k_strlen(label)*8;
        vesa_draw_string(bx+(bw2-tw2)/2,by+4,label,fg2,bg2);

        if (!ds[i]) {
            vesa_draw_string(bx+bw2/2-48,by+22,"desteklenmiyor",0x773333,bg2);
        } else {
            const char* ci;
            switch(dv[i]) {
                case 1:ci="2 renk";break; case 4:ci="64 renk";break;
                case 8:ci="256 renk";break; case 16:ci="65K renk";break;
                case 24:ci="16.7M renk";break; case 32:ci="16.7M+Alpha";break;
                default:ci="";break;
            }
            int ciw=k_strlen(ci)*8;
            vesa_draw_string(bx+(bw2-ciw)/2,by+22,ci,COLOR_TEXT_GREY,bg2);
        }
    }

    /* Onizleme */
    int gy=wy+325;
    char seltxt[12]; k_itoa(settings_selected_depth,seltxt,10);
    int stl=k_strlen(seltxt);
    seltxt[stl]='-'; seltxt[stl+1]='b'; seltxt[stl+2]='i';
    seltxt[stl+3]='t'; seltxt[stl+4]='\0';
    vesa_draw_string(wx+20,gy,"Onizleme:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+100,gy,seltxt,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    gy+=20;
    int gw=ww-40;
    for (int px=0; px<gw; px++) {
        uint32_t r2=(uint32_t)((px*255)/gw);
        uint32_t g2=255-r2;
        uint32_t b2=(uint32_t)((px*128)/gw)+64;
        uint32_t col=(r2<<16)|(g2<<8)|b2;
        uint32_t pc=vesa_preview_color(col,settings_selected_depth);
        for (int py=0;py<16;py++) vesa_putpixel_raw(wx+20+px,gy+py,pc);
    }
    int boxy=gy+18;
    uint32_t tc2[]={0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0x0078D4,0xFFFFFF};
    for (int i=0;i<8;i++) {
        uint32_t tc=vesa_preview_color(tc2[i],settings_selected_depth);
        int bxx=wx+20+i*(gw/8);
        for (int py2=0;py2<14;py2++)
            for (int px2=0;px2<gw/8-2;px2++)
                vesa_putpixel_raw(bxx+px2,boxy+py2,tc);
    }
    vesa_draw_rect_outline(wx+20,gy,gw,16,COLOR_WINDOW_BORDER);
    vesa_draw_rect_outline(wx+20,boxy,gw,14,COLOR_WINDOW_BORDER);

    /* Uygula */
    int abx=wx+ww/2-80, aby=wy+wh-45;
    int ahov=(mx>=abx&&mx<abx+160&&my>=aby&&my<aby+35);
    uint32_t abg=ahov?COLOR_START_HOVER:COLOR_ACCENT;
    vesa_fill_rect(abx,aby,160,35,abg);
    vesa_draw_rect_outline(abx,aby,160,35,0x005599);
    vesa_draw_string(abx+44,aby+10,"Uygula",COLOR_TEXT_WHITE,abg);
}

/* ======== HZ AYARLARI PENCERESI ======== */
static const int hz_vals[9]  = {30, 50, 60, 75, 100, 120, 144, 165, 240};
static const int hz_sup[9]   = { 1,  1,  1,  1,   0,   0,   0,   0,   0};

static void show_hz_window(int mx, int my) {
    int wx=140,wy=40,ww=520,wh=480;

    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran Ayarlari - Yenileme Hizi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    /* Tab butonlari */
    int cd_tab_hover = (mx>=wx+10 && mx<wx+170 && my>=wy+38 && my<wy+66);
    uint32_t cd_tab_bg = cd_tab_hover ? COLOR_BUTTON_HOVER : COLOR_BUTTON;
    vesa_fill_rect(wx+10,wy+38,160,28,cd_tab_bg);
    vesa_draw_string(wx+18,wy+44,"<< Renk Derinligi",COLOR_TEXT_WHITE,cd_tab_bg);
    vesa_fill_rect(wx+180,wy+38,160,28,COLOR_ACCENT);
    vesa_draw_string(wx+196,wy+44,"Yenileme Hizi",COLOR_TEXT_WHITE,COLOR_ACCENT);

    /* Monitor bilgisi */
    vesa_fill_rect(wx+20,wy+75,ww-40,75,0x1A1A2A);
    vesa_draw_rect_outline(wx+20,wy+75,ww-40,75,COLOR_WINDOW_BORDER);
    draw_icon_display(wx+30,wy+82);

    vesa_draw_string(wx+65,wy+82,"Aktif Hz :",COLOR_TEXT_GREY,0x1A1A2A);
    char ahz[10]; k_itoa(current_hz,ahz,10);
    int al=k_strlen(ahz);
    ahz[al]=' '; ahz[al+1]='H'; ahz[al+2]='z'; ahz[al+3]='\0';
    vesa_draw_string(wx+165,wy+82,ahz,COLOR_TEXT_GREEN,0x1A1A2A);

    vesa_draw_string(wx+65,wy+100,"Desteklenen:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+170,wy+100,"30 - 75 Hz",COLOR_TEXT_WHITE,0x1A1A2A);

    vesa_draw_string(wx+65,wy+118,"Monitor :",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+170,wy+118,"VBE 75Hz max",COLOR_TEXT_YELLOW,0x1A1A2A);

    vesa_draw_string(wx+20,wy+160,"Yenileme Hizi Secin:",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);

    /* 9 buton (3x3) */
    for (int i = 0; i < 9; i++) {
        int bx=wx+20+(i%3)*165, by=wy+180+(i/3)*55;
        int bw2=150,bh2=45;
        int hov=(mx>=bx&&mx<bx+bw2&&my>=by&&my<by+bh2);
        int sel=(hz_vals[i]==selected_hz);
        int act=(hz_vals[i]==current_hz);
        int sup=hz_sup[i];

        uint32_t bg2,fg2;
        if (!sup)     { bg2=0x1A1A2A; fg2=0x555555; }
        else if (sel) { bg2=COLOR_ACCENT; fg2=COLOR_TEXT_WHITE; }
        else if (hov) { bg2=COLOR_BUTTON_HOVER; fg2=COLOR_TEXT_WHITE; }
        else          { bg2=COLOR_BUTTON; fg2=COLOR_TEXT_WHITE; }

        vesa_fill_rect(bx,by,bw2,bh2,bg2);
        vesa_draw_rect_outline(bx,by,bw2,bh2,sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if (act&&sup) vesa_draw_rect_outline(bx+1,by+1,bw2-2,bh2-2,COLOR_TEXT_GREEN);

        /* Hz yazisi */
        char label[12]; k_itoa(hz_vals[i],label,10);
        int ll=k_strlen(label);
        label[ll]=' '; label[ll+1]='H'; label[ll+2]='z'; label[ll+3]='\0';
        int tw2=k_strlen(label)*8;
        vesa_draw_string(bx+(bw2-tw2)/2,by+6,label,fg2,bg2);

        /* Alt bilgi */
        if (!sup) {
            /* Uyari ikonu */
            vesa_draw_string(bx+bw2/2-52,by+24,"! desteklenmiyor",0xFF6644,bg2);
        } else {
            const char* desc;
            switch(hz_vals[i]) {
                case 30:  desc="Sinema modu"; break;
                case 50:  desc="PAL standart"; break;
                case 60:  desc="Varsayilan"; break;
                case 75:  desc="Maksimum"; break;
                default:  desc=""; break;
            }
            int dw=k_strlen(desc)*8;
            vesa_draw_string(bx+(bw2-dw)/2,by+24,desc,COLOR_TEXT_GREY,bg2);
        }
    }

    /* Secili Hz bilgisi */
    int iy=wy+355;
    vesa_draw_string(wx+20,iy,"Secili:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char shz[10]; k_itoa(selected_hz,shz,10);
    int sl=k_strlen(shz);
    shz[sl]=' '; shz[sl+1]='H'; shz[sl+2]='z'; shz[sl+3]='\0';
    vesa_draw_string(wx+80,iy,shz,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);

    if (!hz_sup[0]) { /* dummy */ }
    /* Uyari mesaji */
    int sel_supported = 0;
    for (int i=0; i<9; i++)
        if (hz_vals[i]==selected_hz && hz_sup[i]) sel_supported=1;

    if (!sel_supported) {
        vesa_fill_rect(wx+20,iy+25,ww-40,22,0x3A1A1A);
        vesa_draw_rect_outline(wx+20,iy+25,ww-40,22,0x882222);
        vesa_draw_string(wx+30,iy+28,"! Monitor bu Hz degerini desteklemiyor",COLOR_TEXT_RED,0x3A1A1A);
    } else {
        vesa_fill_rect(wx+20,iy+25,ww-40,22,0x1A2A1A);
        vesa_draw_rect_outline(wx+20,iy+25,ww-40,22,0x228822);
        vesa_draw_string(wx+30,iy+28,"Monitor bu Hz degerini destekliyor",COLOR_TEXT_GREEN,0x1A2A1A);
    }

    /* Gorsel hz onizleme - hareketli cubuk */
    int vy=iy+55;
    vesa_draw_string(wx+20,vy,"Onizleme:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_fill_rect(wx+20,vy+18,ww-40,20,0x1A1A2A);
    vesa_draw_rect_outline(wx+20,vy+18,ww-40,20,COLOR_WINDOW_BORDER);

    /* Hizi temsil eden cubuklar */
    int bar_count = selected_hz / 10;
    if (bar_count > (ww-60)/6) bar_count = (ww-60)/6;
    for (int i=0; i<bar_count; i++) {
        uint32_t bc = sel_supported ? COLOR_ACCENT : 0x884444;
        vesa_fill_rect(wx+24+i*6, vy+20, 4, 16, bc);
    }

    /* Uygula */
    int abx=wx+ww/2-80, aby=wy+wh-45;
    int ahov=(mx>=abx&&mx<abx+160&&my>=aby&&my<aby+35);
    uint32_t abg2=ahov?COLOR_START_HOVER:COLOR_ACCENT;
    vesa_fill_rect(abx,aby,160,35,abg2);
    vesa_draw_rect_outline(abx,aby,160,35,0x005599);
    vesa_draw_string(abx+44,aby+10,"Uygula",COLOR_TEXT_WHITE,abg2);
}

/* ======== HATA POPUP ======== */
static void show_error_popup(int mx, int my) {
    int pw=380, ph=220;
    int px=vesa_get_width()/2-pw/2, py=vesa_get_height()/2-ph/2;

    /* Koyu overlay */
    for (int y2=0; y2<vesa_get_height(); y2++)
        for (int x2=0; x2<vesa_get_width(); x2++) {
            /* Sadece popup disini karart */
            if (x2<px || x2>=px+pw || y2<py || y2>=py+ph) {
                uint32_t old = backbuffer[y2*vesa_get_width()+x2];
                uint32_t r=(old>>16)&0xFF, g=(old>>8)&0xFF, b=old&0xFF;
                r/=3; g/=3; b/=3;
                backbuffer[y2*vesa_get_width()+x2] = (r<<16)|(g<<8)|b;
            }
        }

    /* Popup pencere */
    vesa_fill_rect(px,py,pw,ph,COLOR_WINDOW_BG);
    vesa_fill_rect(px,py,pw,32,0x881122);
    vesa_draw_rect_outline(px,py,pw,ph,0xAA2233);
    vesa_draw_string(px+16,py+8,"Hata",COLOR_TEXT_WHITE,0x881122);
    draw_close_button(px+pw-24,py+8,COLOR_CLOSE_BTN);

    /* Uyari ikonu - buyuk daire */
    int icx=px+pw/2, icy=py+75;
    for (int dy=-18; dy<=18; dy++)
        for (int dx2=-18; dx2<=18; dx2++)
            if (dx2*dx2+dy*dy<=324 && dx2*dx2+dy*dy>=256)
                vesa_putpixel(icx+dx2,icy+dy,COLOR_TEXT_RED);
    /* ! isareti */
    vesa_fill_rect(icx-2,icy-10,4,12,COLOR_TEXT_RED);
    vesa_fill_rect(icx-2,icy+5,4,4,COLOR_TEXT_RED);

    /* Hata mesaji */
    vesa_draw_string(px+40,py+105,"Bu yenileme hizi monitorunuz",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(px+40,py+123,"tarafindan desteklenmiyor!",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);

    /* Secilen Hz */
    char shz2[16]; k_strcpy(shz2, "Secilen: ");
    int sl=k_strlen(shz2);
    k_itoa(selected_hz,shz2+sl,10);
    int sl2=k_strlen(shz2);
    shz2[sl2]=' '; shz2[sl2+1]='H'; shz2[sl2+2]='z'; shz2[sl2+3]='\0';
    vesa_draw_string(px+40,py+145,shz2,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);

    vesa_draw_string(px+40,py+163,"Desteklenen: 30, 50, 60, 75 Hz",COLOR_TEXT_GREY,COLOR_WINDOW_BG);

    /* Tamam butonu */
    int obx=px+pw/2-60, oby=py+ph-42;
    int ohov=(mx>=obx&&mx<obx+120&&my>=oby&&my<oby+32);
    uint32_t obg=ohov?COLOR_START_HOVER:COLOR_ACCENT;
    vesa_fill_rect(obx,oby,120,32,obg);
    vesa_draw_rect_outline(obx,oby,120,32,0x005599);
    vesa_draw_string(obx+36,oby+8,"Tamam",COLOR_TEXT_WHITE,obg);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);
    vesa_draw_string(320,290,"Kapatiliyor...",COLOR_TEXT_WHITE,COLOR_BLACK);
    vesa_copy_buffer();
    outb(0x604,0x2000); outb(0xB004,0x2000); outb(0x4004,0x3400);
    __asm__ __volatile__("cli; hlt");
}

/* ======== ANA GUI DONGUSU ======== */
void gui_run(void) {
    mouse_state_t ms;
    int sh=vesa_get_height(), sw=vesa_get_width();
    settings_selected_depth = vesa_get_depth();
    selected_hz = current_hz;

    while (1) {
        mouse_poll(&ms);

        vesa_fill_screen(COLOR_BG);
        draw_desktop_icons();

        if (window_open==1) show_sysinfo_window();
        if (window_open==2) show_about_window();
        if (window_open==3) show_settings_window(ms.x,ms.y);
        if (window_open==4) show_hz_window(ms.x,ms.y);
        if (window_open==5) show_error_popup(ms.x,ms.y);

        draw_taskbar();
        if (start_open) draw_start_menu(ms.x,ms.y);

        mouse_draw_cursor(ms.x,ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;

        /* ---- HATA POPUP ---- */
        if (window_open==5) {
            int pw2=380,ph2=220;
            int px2=sw/2-pw2/2, py2=sh/2-ph2/2;
            /* Kapat butonu */
            if (ms.x>=px2+pw2-24&&ms.x<px2+pw2-8&&ms.y>=py2+8&&ms.y<py2+24)
                { window_open=4; continue; }
            /* Tamam butonu */
            int obx=px2+pw2/2-60, oby=py2+ph2-42;
            if (ms.x>=obx&&ms.x<obx+120&&ms.y>=oby&&ms.y<oby+32)
                { window_open=4; continue; }
            continue;
        }

        /* ---- PENCERE KAPAT ---- */
        if (window_open==1) {
            if (ms.x>=160+480-24&&ms.x<160+480-8&&ms.y>=60+8&&ms.y<60+24)
                { window_open=0; continue; }
        }
        if (window_open==2) {
            if (ms.x>=200+400-24&&ms.x<200+400-8&&ms.y>=120+8&&ms.y<120+24)
                { window_open=0; continue; }
        }

        /* ---- RENK AYARLARI ---- */
        if (window_open==3) {
            int wx3=140,wy3=40,ww3=520,wh3=480;
            if (ms.x>=wx3+ww3-24&&ms.x<wx3+ww3-8&&ms.y>=wy3+8&&ms.y<wy3+24)
                { window_open=0; continue; }

            /* Hz tab */
            if (ms.x>=wx3+180&&ms.x<wx3+340&&ms.y>=wy3+38&&ms.y<wy3+66)
                { window_open=4; selected_hz=current_hz; continue; }

            /* Derinlik butonlari */
            for (int i=0;i<9;i++) {
                int bx=wx3+20+(i%3)*165, by=wy3+160+(i/3)*50;
                if (ds[i]&&ms.x>=bx&&ms.x<bx+150&&ms.y>=by&&ms.y<by+40)
                    { settings_selected_depth=dv[i]; break; }
            }
            /* Uygula */
            int abx=wx3+ww3/2-80, aby=wy3+wh3-45;
            if (ms.x>=abx&&ms.x<abx+160&&ms.y>=aby&&ms.y<aby+35) {
                vesa_set_depth(settings_selected_depth);
                window_open=0;
            }
            continue;
        }

        /* ---- HZ AYARLARI ---- */
        if (window_open==4) {
            int wx3=140,wy3=40,ww3=520,wh3=480;
            if (ms.x>=wx3+ww3-24&&ms.x<wx3+ww3-8&&ms.y>=wy3+8&&ms.y<wy3+24)
                { window_open=0; continue; }

            /* Renk tab */
            if (ms.x>=wx3+10&&ms.x<wx3+170&&ms.y>=wy3+38&&ms.y<wy3+66)
                { window_open=3; settings_selected_depth=vesa_get_depth(); continue; }

            /* Hz butonlari */
            for (int i=0;i<9;i++) {
                int bx=wx3+20+(i%3)*165, by=wy3+180+(i/3)*55;
                if (ms.x>=bx&&ms.x<bx+150&&ms.y>=by&&ms.y<by+45)
                    { selected_hz=hz_vals[i]; break; }
            }

            /* Uygula */
            int abx=wx3+ww3/2-80, aby=wy3+wh3-45;
            if (ms.x>=abx&&ms.x<abx+160&&ms.y>=aby&&ms.y<aby+35) {
                /* Destekleniyor mu kontrol et */
                int sup2=0;
                for (int i=0;i<9;i++)
                    if (hz_vals[i]==selected_hz && hz_sup[i]) sup2=1;

                if (sup2) {
                    current_hz = selected_hz;
                    window_open = 0;
                } else {
                    window_open = 5; /* Hata popup */
                }
            }
            continue;
        }

        /* ---- BASLAT ---- */
        if (ms.x>=4&&ms.x<84&&ms.y>=sh-36&&ms.y<sh-4)
            { start_open=!start_open; window_open=0; continue; }

        /* Gear ikonu */
        if (ms.x>=sw-85&&ms.x<sw-55&&ms.y>=sh-32&&ms.y<sh-8)
            { window_open=3; settings_selected_depth=vesa_get_depth(); start_open=0; continue; }

        /* ---- BASLAT MENU ---- */
        if (start_open) {
            int smy=sh-40-220;
            for (int i=0;i<5;i++) {
                int iy=smy+50+i*32;
                if (ms.x>=0&&ms.x<240&&ms.y>=iy&&ms.y<iy+32) {
                    start_open=0;
                    if (i==0) gui_terminal();
                    else if (i==1) window_open=1;
                    else if (i==2) window_open=2;
                    else if (i==3) outb(0x64,0xFE);
                    else if (i==4) do_shutdown();
                    break;
                }
            }
            if (ms.x>240||ms.y<(sh-40-220)) start_open=0;
            continue;
        }

        /* ---- MASAUSTU IKONLARI ---- */
        for (int i=0;i<DICON_COUNT;i++) {
            int ix=dicons[i].x, iy=dicons[i].y;
            if (ms.x>=ix&&ms.x<ix+60&&ms.y>=iy&&ms.y<iy+50) {
                if (dicons[i].id==1) window_open=1;
                else if (dicons[i].id==2) gui_terminal();
                else if (dicons[i].id==3) window_open=2;
                else if (dicons[i].id==4) { window_open=3; settings_selected_depth=vesa_get_depth(); }
                start_open=0; break;
            }
        }
    }
}
