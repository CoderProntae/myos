#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"

static int start_open = 0;
/* 0=yok 1=sysinfo 2=about 3=renk 4=hz 5=hata_renk 6=hata_hz */
static int window_open = 0;
static int settings_selected_depth = 32;
static int current_hz = 60;
static int selected_hz = 60;

/* ======== MONITOR ALGILAMA ======== */
static int mon_depth_sup[9]; /* monitor destekliyor mu */
static int mon_hz_sup[9];
static char mon_name[32];
static int mon_max_bpp = 32;
static int mon_max_hz = 75;
static int mon_detected = 0;

static const int dv[9] = {1, 4, 8, 16, 24, 30, 32, 36, 48};
static const int hz_vals[9] = {30, 50, 60, 75, 100, 120, 144, 165, 240};

static void detect_monitor(void) {
    if (mon_detected) return;

    int hw_bpp = vesa_get_bpp();
    mon_max_bpp = hw_bpp;

    /* Monitor adi */
    k_strcpy(mon_name, "VBE Uyumlu Monitor");

    /*
     * Renk derinligi destegi:
     * - 1,4,8,16,24,32: standart 8-bit panel destekler
     * - 30-bit: 10-bit panel gerekir (1.07 milyar renk)
     * - 36-bit: 12-bit panel gerekir
     * - 48-bit: 16-bit panel gerekir
     *
     * Standart monitor = 8-bit panel = max 32-bit (8+8+8+8)
     */
    for (int i = 0; i < 9; i++) {
        int d = dv[i];
        if (d <= 32 && d != 30) {
            /* Standart derinlikler: 8-bit panel yeterli */
            mon_depth_sup[i] = (d <= hw_bpp || d < 24) ? 1 : 0;
            /* 1,4,8,16 her zaman simulasyon ile calisir */
            if (d <= 16) mon_depth_sup[i] = 1;
            if (d == 24 && hw_bpp >= 24) mon_depth_sup[i] = 1;
            if (d == 32 && hw_bpp >= 32) mon_depth_sup[i] = 1;
        } else {
            /* 30,36,48-bit ozel panel gerektirir */
            mon_depth_sup[i] = 0;
        }
    }

    /*
     * Yenileme hizi destegi:
     * VBE standart monitor: 30-75 Hz
     * Gaming monitor: 100+ Hz
     * Bu monitor: max 75 Hz (standart VBE)
     */
    mon_max_hz = 75;
    for (int i = 0; i < 9; i++) {
        mon_hz_sup[i] = (hz_vals[i] <= mon_max_hz) ? 1 : 0;
    }

    mon_detected = 1;
}

/* OS her derinligi ve Hz destekler (yazilimsal) */
static const char* depth_info(int d) {
    switch(d) {
        case 1:  return "2 renk";
        case 4:  return "64 renk";
        case 8:  return "256 renk";
        case 16: return "65,536 renk";
        case 24: return "16.7M renk";
        case 30: return "1.07 Milyar renk";
        case 32: return "16.7M + Alpha";
        case 36: return "68.7 Milyar renk";
        case 48: return "281 Trilyon renk";
        default: return "";
    }
}

static const char* hz_info(int h) {
    switch(h) {
        case 30:  return "Sinema modu";
        case 50:  return "PAL standart";
        case 60:  return "Varsayilan";
        case 75:  return "Gelismis";
        case 100: return "Yuksek";
        case 120: return "Oyun modu";
        case 144: return "Gaming";
        case 165: return "Gaming+";
        case 240: return "E-spor";
        default:  return "";
    }
}

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
                gt_buf[r][c] = gt_buf[r+1][c];
        k_memset(gt_buf[GT_ROWS-1], 0, GT_COLS + 1);
        gt_cy--;
    }
}

static void gt_putc(char c) {
    if (c == '\n') { gt_cx=0; gt_cy++; gt_scroll(); }
    else if (c == '\b') { if (gt_cx>0) { gt_cx--; gt_buf[gt_cy][gt_cx]=' '; } }
    else if (gt_cx < GT_COLS) {
        gt_buf[gt_cy][gt_cx] = c;
        gt_cx++;
        if (gt_cx >= GT_COLS) { gt_cx=0; gt_cy++; gt_scroll(); }
    }
}
static void gt_puts(const char* s) { while (*s) gt_putc(*s++); }

static void gt_render(void) {
    int sw=vesa_get_width(), sh=vesa_get_height();
    int wx=20, wy=10, ww=sw-40, wh=sh-50;
    vesa_fill_screen(COLOR_BG);
    vesa_fill_rect(wx,wy,ww,wh,COLOR_TERMINAL_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Terminal - MyOS",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    vesa_fill_rect(wx+ww-28,wy+4,24,24,COLOR_CLOSE_BTN);
    for (int i=0;i<10;i++) {
        vesa_putpixel(wx+ww-22+i,wy+10+i,COLOR_TEXT_WHITE);
        vesa_putpixel(wx+ww-13-i,wy+10+i,COLOR_TEXT_WHITE);
    }
    for (int r=0;r<GT_ROWS;r++)
        for (int c=0;c<GT_COLS;c++) {
            char ch=gt_buf[r][c];
            if (ch && ch!=' ') {
                uint32_t fg = c<8 ? COLOR_TEXT_GREEN : COLOR_TEXT_GREY;
                vesa_draw_char(GT_OX+c*8, GT_OY+r*16, ch, fg, COLOR_TERMINAL_BG);
            }
        }
    vesa_fill_rect(GT_OX+gt_cx*8, GT_OY+gt_cy*16, 8, 16, 0x888888);
    vesa_copy_buffer();
}

static void gt_prompt(void) {
    gt_puts("myos:~$ ");
    gt_cmd_len=0; k_memset(gt_cmd,0,256);
}

static int gt_process_cmd(void) {
    gt_putc('\n');
    const char* c=gt_cmd;
    while (*c==' ') c++;
    if (k_strlen(c)==0) return 0;
    if (k_strcmp(c,"exit")==0) return 1;
    if (k_strcmp(c,"clear")==0) { gt_clear_buf(); return 0; }
    if (k_strcmp(c,"help")==0) {
        gt_puts("  help clear echo time sysinfo exit reboot\n"); return 0;
    }
    if (k_strcmp(c,"time")==0) {
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
    if (k_strcmp(c,"sysinfo")==0) {
        char vendor[13]; uint32_t ebx2,ecx2,edx2;
        __asm__ __volatile__("cpuid":"=b"(ebx2),"=c"(ecx2),"=d"(edx2):"a"(0));
        *((uint32_t*)&vendor[0])=ebx2;*((uint32_t*)&vendor[4])=edx2;
        *((uint32_t*)&vendor[8])=ecx2; vendor[12]='\0';
        gt_puts("  CPU   : "); gt_puts(vendor); gt_putc('\n');
        gt_puts("  Video : VESA 800x600\n");
        char db[10]; k_itoa(vesa_get_depth(),db,10);
        gt_puts("  Renk  : "); gt_puts(db); gt_puts("-bit\n");
        char hb[10]; k_itoa(current_hz,hb,10);
        gt_puts("  Hz    : "); gt_puts(hb); gt_puts(" Hz\n");
        gt_puts("  Monitor: "); gt_puts(mon_name); gt_putc('\n');
        return 0;
    }
    if (k_strncmp(c,"echo ",5)==0) { gt_puts("  "); gt_puts(c+5); gt_putc('\n'); return 0; }
    if (k_strcmp(c,"reboot")==0) { outb(0x64,0xFE); return 0; }
    gt_puts("  Bilinmeyen: "); gt_puts(c); gt_puts("\n  'help' yazin.\n"); return 0;
}

static void gui_terminal(void) {
    gt_clear_buf();
    gt_puts("MyOS Terminal v0.3\n");
    gt_puts("Cikis icin 'exit' yazin.\n\n");
    gt_prompt(); gt_render();
    while (1) {
        char c=keyboard_getchar();
        if (c=='\n') {
            gt_cmd[gt_cmd_len]='\0';
            if (gt_process_cmd()) return;
            gt_prompt();
        } else if (c=='\b') {
            if (gt_cmd_len>0) { gt_cmd_len--; gt_cmd[gt_cmd_len]='\0'; gt_putc('\b'); }
        } else if (c>=32 && c<127 && gt_cmd_len<250) {
            gt_cmd[gt_cmd_len++]=c; gt_putc(c);
        }
        gt_render();
    }
}

/* ======== MASAUSTU ======== */
typedef struct { int x,y; const char* label; int id; } dicon_t;
static dicon_t dicons[] = {
    {30,40,"Bilgisayarim",1}, {30,120,"Terminal",2},
    {30,200,"Hakkinda",3}, {30,280,"Ekran Ayar",4},
};
#define DICON_COUNT 4

static void draw_desktop_icons(void) {
    for (int i=0;i<DICON_COUNT;i++) {
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
    uint32_t sc=start_open?COLOR_START_HOVER:COLOR_START_BTN;
    vesa_fill_rect(4,ty+4,80,32,sc);
    draw_windows_logo(12,ty+10,COLOR_TEXT_WHITE);
    vesa_draw_string(32,ty+12,"Baslat",COLOR_TEXT_WHITE,sc);
    char clock[6]; get_time(clock);
    vesa_draw_string(sw-48,ty+12,clock,COLOR_TEXT_WHITE,COLOR_TASKBAR);
    draw_icon_gear(sw-75,ty+12,COLOR_TEXT_GREY);
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
    for (int i=0;i<5;i++) {
        int iy=smy+50+i*32;
        int hov=(mx>=0&&mx<240&&my>=iy&&my<iy+32);
        uint32_t ibg=hov?COLOR_MENU_HOVER:COLOR_MENU_BG;
        vesa_fill_rect(1,iy,238,32,ibg);
        vesa_draw_string(20,iy+8,items[i],COLOR_TEXT_WHITE,ibg);
        if (i==4) draw_icon_power(206,iy+4);
    }
}

static void show_sysinfo_window(void) {
    int wx=160,wy=50,ww=480,wh=440;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Sistem Bilgisi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    char vendor[13]; uint32_t ebx,ecx,edx;
    __asm__ __volatile__("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    *((uint32_t*)&vendor[0])=ebx;*((uint32_t*)&vendor[4])=edx;
    *((uint32_t*)&vendor[8])=ecx; vendor[12]='\0';

    int cy=wy+50;
    draw_icon_computer(wx+20,cy);
    vesa_draw_string(wx+60,cy+4,"CPU:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+120,cy+4,vendor,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);
    cy+=35;
    vesa_draw_string(wx+60,cy,"Mimari:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+140,cy,"i686 32-bit",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=25;
    vesa_draw_string(wx+60,cy,"Video:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char res[20]; k_itoa(vesa_get_width(),res,10);
    int rl=k_strlen(res); res[rl]='x'; k_itoa(vesa_get_height(),res+rl+1,10);
    vesa_draw_string(wx+140,cy,res,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=25;
    vesa_draw_string(wx+60,cy,"Renk:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char db[10]; k_itoa(vesa_get_depth(),db,10);
    int dl4=k_strlen(db);
    db[dl4]='-'; db[dl4+1]='b'; db[dl4+2]='i'; db[dl4+3]='t'; db[dl4+4]='\0';
    vesa_draw_string(wx+140,cy,db,COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    cy+=25;
    vesa_draw_string(wx+60,cy,"Hz:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char hb[10]; k_itoa(current_hz,hb,10);
    int hl=k_strlen(hb);
    hb[hl]=' '; hb[hl+1]='H'; hb[hl+2]='z'; hb[hl+3]='\0';
    vesa_draw_string(wx+140,cy,hb,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);

    /* Monitor bilgisi */
    cy+=40;
    vesa_fill_rect(wx+20,cy,ww-40,80,0x1A1A2A);
    vesa_draw_rect_outline(wx+20,cy,ww-40,80,COLOR_WINDOW_BORDER);
    draw_icon_display(wx+30,cy+10);
    vesa_draw_string(wx+65,cy+10,"Monitor:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+155,cy+10,mon_name,COLOR_TEXT_WHITE,0x1A1A2A);
    char mbpp[16]; k_itoa(mon_max_bpp,mbpp,10);
    int ml=k_strlen(mbpp);
    mbpp[ml]='-'; mbpp[ml+1]='b'; mbpp[ml+2]='i'; mbpp[ml+3]='t';
    mbpp[ml+4]=' '; mbpp[ml+5]='m'; mbpp[ml+6]='a'; mbpp[ml+7]='x'; mbpp[ml+8]='\0';
    vesa_draw_string(wx+65,cy+30,"Panel:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+155,cy+30,mbpp,COLOR_TEXT_GREEN,0x1A1A2A);
    char mhz[16]; k_itoa(mon_max_hz,mhz,10);
    int mhl=k_strlen(mhz);
    mhz[mhl]=' '; mhz[mhl+1]='H'; mhz[mhl+2]='z'; mhz[mhl+3]=' ';
    mhz[mhl+4]='m'; mhz[mhl+5]='a'; mhz[mhl+6]='x'; mhz[mhl+7]='\0';
    vesa_draw_string(wx+65,cy+50,"Yenileme:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+155,cy+50,mhz,COLOR_TEXT_GREEN,0x1A1A2A);

    cy+=95;
    vesa_draw_string(wx+60,cy,"Versiyon:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+155,cy,"MyOS v0.3",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
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
static void show_settings_window(int mx, int my) {
    int wx=130,wy=30,ww=540,wh=510;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran - Renk Derinligi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    /* Tablar */
    vesa_fill_rect(wx+10,wy+38,160,26,COLOR_ACCENT);
    vesa_draw_string(wx+20,wy+43,"Renk Derinligi",COLOR_TEXT_WHITE,COLOR_ACCENT);
    int ht_hov=(mx>=wx+180&&mx<wx+340&&my>=wy+38&&my<wy+64);
    vesa_fill_rect(wx+180,wy+38,160,26,ht_hov?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    vesa_draw_string(wx+194,wy+43,"Yenileme Hizi >>",COLOR_TEXT_WHITE,ht_hov?COLOR_BUTTON_HOVER:COLOR_BUTTON);

    /* Monitor kutusu */
    vesa_fill_rect(wx+15,wy+70,ww-30,45,0x1A1A2A);
    vesa_draw_rect_outline(wx+15,wy+70,ww-30,45,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+25,wy+75,"Monitor:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+110,wy+75,mon_name,COLOR_TEXT_WHITE,0x1A1A2A);
    char mbpp[16]; k_itoa(mon_max_bpp,mbpp,10);
    int mbl=k_strlen(mbpp);
    mbpp[mbl]='-'; mbpp[mbl+1]='b'; mbpp[mbl+2]='i'; mbpp[mbl+3]='t';
    mbpp[mbl+4]=' '; mbpp[mbl+5]='p'; mbpp[mbl+6]='a'; mbpp[mbl+7]='n';
    mbpp[mbl+8]='e'; mbpp[mbl+9]='l'; mbpp[mbl+10]='\0';
    vesa_draw_string(wx+25,wy+93,"Panel:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+110,wy+93,mbpp,COLOR_TEXT_GREEN,0x1A1A2A);

    vesa_draw_string(wx+15,wy+122,"Renk Derinligi  (OS tum modlari destekler):",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);

    /* 9 buton - HEPSI TIKLANABILIR */
    for (int i=0;i<9;i++) {
        int bx=wx+15+(i%3)*175, by=wy+142+(i/3)*52;
        int bw2=165,bh2=46;
        int hov=(mx>=bx&&mx<bx+bw2&&my>=by&&my<by+bh2);
        int sel=(dv[i]==settings_selected_depth);
        int act=(dv[i]==vesa_get_depth());
        int msup=mon_depth_sup[i];

        uint32_t bg2,fg2;
        if (sel)      { bg2=COLOR_ACCENT; fg2=COLOR_TEXT_WHITE; }
        else if (hov) { bg2=COLOR_BUTTON_HOVER; fg2=COLOR_TEXT_WHITE; }
        else          { bg2=COLOR_BUTTON; fg2=COLOR_TEXT_WHITE; }

        vesa_fill_rect(bx,by,bw2,bh2,bg2);
        vesa_draw_rect_outline(bx,by,bw2,bh2,sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if (act) vesa_draw_rect_outline(bx+1,by+1,bw2-2,bh2-2,COLOR_TEXT_GREEN);

        /* Derinlik yazisi */
        char label[12]; k_itoa(dv[i],label,10);
        int ll=k_strlen(label);
        label[ll]='-'; label[ll+1]='b'; label[ll+2]='i'; label[ll+3]='t'; label[ll+4]='\0';
        vesa_draw_string(bx+8,by+4,label,fg2,bg2);

        /* OS: her zaman destekler */
        vesa_draw_string(bx+70,by+4,"OS:",COLOR_TEXT_GREY,bg2);
        vesa_fill_rect(bx+96,by+6,8,8,COLOR_TEXT_GREEN); /* yesil kare */

        /* Monitor destegi */
        vesa_draw_string(bx+8,by+22,"Mon:",COLOR_TEXT_GREY,bg2);
        if (msup) {
            vesa_fill_rect(bx+44,by+24,8,8,COLOR_TEXT_GREEN);
            vesa_draw_string(bx+56,by+22,depth_info(dv[i]),COLOR_TEXT_GREY,bg2);
        } else {
            vesa_fill_rect(bx+44,by+24,8,8,COLOR_TEXT_RED);
            vesa_draw_string(bx+56,by+22,"Panel yetersiz",0xFF8844,bg2);
        }
    }

    /* Onizleme */
    int gy=wy+305;
    char seltxt[12]; k_itoa(settings_selected_depth,seltxt,10);
    int stl=k_strlen(seltxt);
    seltxt[stl]='-'; seltxt[stl+1]='b'; seltxt[stl+2]='i';
    seltxt[stl+3]='t'; seltxt[stl+4]='\0';
    vesa_draw_string(wx+15,gy,"Onizleme:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+95,gy,seltxt,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);

    /* Gradyan */
    gy+=18;
    int gw=ww-30;
    for (int px=0;px<gw;px++) {
        uint32_t r2=(uint32_t)((px*255)/gw);
        uint32_t g2=255-r2;
        uint32_t b2=(uint32_t)((px*128)/gw)+64;
        uint32_t col=(r2<<16)|(g2<<8)|b2;
        uint32_t pc=vesa_preview_color(col,settings_selected_depth);
        for (int py=0;py<14;py++) vesa_putpixel_raw(wx+15+px,gy+py,pc);
    }
    /* Renk kutuculari */
    int boxy=gy+16;
    uint32_t tc2[]={0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0x0078D4,0xFFFFFF};
    for (int i=0;i<8;i++) {
        uint32_t tc=vesa_preview_color(tc2[i],settings_selected_depth);
        int bxx=wx+15+i*(gw/8);
        for (int py2=0;py2<14;py2++)
            for (int px2=0;px2<gw/8-2;px2++)
                vesa_putpixel_raw(bxx+px2,boxy+py2,tc);
    }
    vesa_draw_rect_outline(wx+15,gy,gw,14,COLOR_WINDOW_BORDER);
    vesa_draw_rect_outline(wx+15,boxy,gw,14,COLOR_WINDOW_BORDER);

    /* Durum mesaji */
    int msup_sel=0;
    for (int i=0;i<9;i++) if (dv[i]==settings_selected_depth) msup_sel=mon_depth_sup[i];
    int sy=boxy+20;
    if (msup_sel) {
        vesa_fill_rect(wx+15,sy,ww-30,20,0x1A2A1A);
        vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x228822);
        vesa_draw_string(wx+25,sy+2,"Monitor bu derinligi destekliyor",COLOR_TEXT_GREEN,0x1A2A1A);
    } else {
        vesa_fill_rect(wx+15,sy,ww-30,20,0x3A1A1A);
        vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x882222);
        vesa_draw_string(wx+25,sy+2,"! Monitor bu derinligi desteklemiyor",COLOR_TEXT_RED,0x3A1A1A);
    }

    /* Uygula */
    int abx=wx+ww/2-80, aby=wy+wh-42;
    int ahov=(mx>=abx&&mx<abx+160&&my>=aby&&my<aby+32);
    vesa_fill_rect(abx,aby,160,32,ahov?COLOR_START_HOVER:COLOR_ACCENT);
    vesa_draw_rect_outline(abx,aby,160,32,0x005599);
    vesa_draw_string(abx+44,aby+8,"Uygula",COLOR_TEXT_WHITE,ahov?COLOR_START_HOVER:COLOR_ACCENT);
}

/* ======== HZ AYARLARI ======== */
static void show_hz_window(int mx, int my) {
    int wx=130,wy=30,ww=540,wh=510;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran - Yenileme Hizi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

    /* Tablar */
    int ct_hov=(mx>=wx+10&&mx<wx+170&&my>=wy+38&&my<wy+64);
    vesa_fill_rect(wx+10,wy+38,160,26,ct_hov?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    vesa_draw_string(wx+16,wy+43,"<< Renk Derinligi",COLOR_TEXT_WHITE,ct_hov?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    vesa_fill_rect(wx+180,wy+38,160,26,COLOR_ACCENT);
    vesa_draw_string(wx+196,wy+43,"Yenileme Hizi",COLOR_TEXT_WHITE,COLOR_ACCENT);

    /* Monitor kutusu */
    vesa_fill_rect(wx+15,wy+70,ww-30,55,0x1A1A2A);
    vesa_draw_rect_outline(wx+15,wy+70,ww-30,55,COLOR_WINDOW_BORDER);
    char ahz[10]; k_itoa(current_hz,ahz,10);
    int al=k_strlen(ahz);
    ahz[al]=' '; ahz[al+1]='H'; ahz[al+2]='z'; ahz[al+3]='\0';
    vesa_draw_string(wx+25,wy+77,"Aktif:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+90,wy+77,ahz,COLOR_TEXT_GREEN,0x1A1A2A);
    char mhz[16]; k_itoa(mon_max_hz,mhz,10);
    int mhl=k_strlen(mhz);
    mhz[mhl]=' '; mhz[mhl+1]='H'; mhz[mhl+2]='z'; mhz[mhl+3]=' ';
    mhz[mhl+4]='m'; mhz[mhl+5]='a'; mhz[mhl+6]='x'; mhz[mhl+7]='\0';
    vesa_draw_string(wx+25,wy+97,"Monitor max:",COLOR_TEXT_GREY,0x1A1A2A);
    vesa_draw_string(wx+130,wy+97,mhz,COLOR_TEXT_YELLOW,0x1A1A2A);

    vesa_draw_string(wx+15,wy+132,"Yenileme Hizi  (OS tum modlari destekler):",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);

    /* 9 buton - HEPSI TIKLANABILIR */
    for (int i=0;i<9;i++) {
        int bx=wx+15+(i%3)*175, by=wy+152+(i/3)*55;
        int bw2=165,bh2=48;
        int hov=(mx>=bx&&mx<bx+bw2&&my>=by&&my<by+bh2);
        int sel=(hz_vals[i]==selected_hz);
        int act=(hz_vals[i]==current_hz);
        int msup=mon_hz_sup[i];

        uint32_t bg2,fg2;
        if (sel)      { bg2=COLOR_ACCENT; fg2=COLOR_TEXT_WHITE; }
        else if (hov) { bg2=COLOR_BUTTON_HOVER; fg2=COLOR_TEXT_WHITE; }
        else          { bg2=COLOR_BUTTON; fg2=COLOR_TEXT_WHITE; }

        vesa_fill_rect(bx,by,bw2,bh2,bg2);
        vesa_draw_rect_outline(bx,by,bw2,bh2,sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if (act) vesa_draw_rect_outline(bx+1,by+1,bw2-2,bh2-2,COLOR_TEXT_GREEN);

        /* Hz yazisi */
        char label[12]; k_itoa(hz_vals[i],label,10);
        int ll=k_strlen(label);
        label[ll]=' '; label[ll+1]='H'; label[ll+2]='z'; label[ll+3]='\0';
        vesa_draw_string(bx+8,by+4,label,fg2,bg2);

        /* OS destegi */
        vesa_draw_string(bx+70,by+4,"OS:",COLOR_TEXT_GREY,bg2);
        vesa_fill_rect(bx+96,by+6,8,8,COLOR_TEXT_GREEN);

        /* Monitor destegi */
        vesa_draw_string(bx+8,by+24,"Mon:",COLOR_TEXT_GREY,bg2);
        if (msup) {
            vesa_fill_rect(bx+44,by+26,8,8,COLOR_TEXT_GREEN);
            vesa_draw_string(bx+56,by+24,hz_info(hz_vals[i]),COLOR_TEXT_GREY,bg2);
        } else {
            vesa_fill_rect(bx+44,by+26,8,8,COLOR_TEXT_RED);
            vesa_draw_string(bx+56,by+24,"Monitor yetersiz",0xFF8844,bg2);
        }
    }

    /* Onizleme cubuk */
    int vy=wy+325;
    char shz[10]; k_itoa(selected_hz,shz,10);
    int sl=k_strlen(shz);
    shz[sl]=' '; shz[sl+1]='H'; shz[sl+2]='z'; shz[sl+3]='\0';
    vesa_draw_string(wx+15,vy,"Secili:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+75,vy,shz,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    vy+=20;
    vesa_fill_rect(wx+15,vy,ww-30,18,0x1A1A2A);
    vesa_draw_rect_outline(wx+15,vy,ww-30,18,COLOR_WINDOW_BORDER);
    int bar_count=selected_hz/5;
    int max_bars=(ww-40)/4;
    if (bar_count>max_bars) bar_count=max_bars;
    int msup_sel=0;
    for (int i=0;i<9;i++) if (hz_vals[i]==selected_hz) msup_sel=mon_hz_sup[i];
    for (int i=0;i<bar_count;i++) {
        uint32_t bc=msup_sel?COLOR_ACCENT:0x884444;
        vesa_fill_rect(wx+18+i*4,vy+2,3,14,bc);
    }

    /* Durum */
    int sy=vy+24;
    if (msup_sel) {
        vesa_fill_rect(wx+15,sy,ww-30,20,0x1A2A1A);
        vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x228822);
        vesa_draw_string(wx+25,sy+2,"Monitor bu yenileme hizini destekliyor",COLOR_TEXT_GREEN,0x1A2A1A);
    } else {
        vesa_fill_rect(wx+15,sy,ww-30,20,0x3A1A1A);
        vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x882222);
        vesa_draw_string(wx+25,sy+2,"! Monitor bu yenileme hizini desteklemiyor",COLOR_TEXT_RED,0x3A1A1A);
    }

    /* Uygula */
    int abx=wx+ww/2-80, aby=wy+wh-42;
    int ahov2=(mx>=abx&&mx<abx+160&&my>=aby&&my<aby+32);
    vesa_fill_rect(abx,aby,160,32,ahov2?COLOR_START_HOVER:COLOR_ACCENT);
    vesa_draw_rect_outline(abx,aby,160,32,0x005599);
    vesa_draw_string(abx+44,aby+8,"Uygula",COLOR_TEXT_WHITE,ahov2?COLOR_START_HOVER:COLOR_ACCENT);
}

/* ======== HATA POPUP ======== */
static char error_title[32];
static char error_line1[64];
static char error_line2[64];
static char error_line3[64];

static void show_error_popup(int mx, int my) {
    int pw=400,ph=240;
    int px=vesa_get_width()/2-pw/2, py=vesa_get_height()/2-ph/2;

    /* Karart */
    for (int y2=0;y2<vesa_get_height();y2++)
        for (int x2=0;x2<vesa_get_width();x2++)
            if (x2<px||x2>=px+pw||y2<py||y2>=py+ph) {
                uint32_t old=backbuffer[y2*vesa_get_width()+x2];
                uint32_t r=(old>>16)&0xFF,g=(old>>8)&0xFF,b=old&0xFF;
                backbuffer[y2*vesa_get_width()+x2]=((r/3)<<16)|((g/3)<<8)|(b/3);
            }

    vesa_fill_rect(px,py,pw,ph,COLOR_WINDOW_BG);
    vesa_fill_rect(px,py,pw,32,0x881122);
    vesa_draw_rect_outline(px,py,pw,ph,0xAA2233);
    vesa_draw_string(px+16,py+8,error_title,COLOR_TEXT_WHITE,0x881122);
    draw_close_button(px+pw-24,py+8,COLOR_CLOSE_BTN);

    /* Uyari ikonu */
    int icx=px+pw/2, icy=py+72;
    for (int dy=-18;dy<=18;dy++)
        for (int dx2=-18;dx2<=18;dx2++)
            if (dx2*dx2+dy*dy<=324&&dx2*dx2+dy*dy>=256)
                vesa_putpixel_raw(icx+dx2,icy+dy,COLOR_TEXT_RED);
    vesa_fill_rect(icx-2,icy-10,4,12,COLOR_TEXT_RED);
    vesa_fill_rect(icx-2,icy+5,4,4,COLOR_TEXT_RED);

    vesa_draw_string(px+30,py+105,error_line1,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(px+30,py+125,error_line2,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    vesa_draw_string(px+30,py+145,error_line3,COLOR_TEXT_GREY,COLOR_WINDOW_BG);

    int obx=px+pw/2-60,oby=py+ph-45;
    int ohov=(mx>=obx&&mx<obx+120&&my>=oby&&my<oby+32);
    vesa_fill_rect(obx,oby,120,32,ohov?COLOR_START_HOVER:COLOR_ACCENT);
    vesa_draw_rect_outline(obx,oby,120,32,0x005599);
    vesa_draw_string(obx+36,oby+8,"Tamam",COLOR_TEXT_WHITE,ohov?COLOR_START_HOVER:COLOR_ACCENT);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);
    vesa_draw_string(320,290,"Kapatiliyor...",COLOR_TEXT_WHITE,COLOR_BLACK);
    vesa_copy_buffer();
    outb(0x604,0x2000); outb(0xB004,0x2000); outb(0x4004,0x3400);
    __asm__ __volatile__("cli; hlt");
}

/* ======== ANA DONGU ======== */
void gui_run(void) {
    mouse_state_t ms;
    int sh=vesa_get_height(), sw=vesa_get_width();

    detect_monitor();
    settings_selected_depth=vesa_get_depth();
    selected_hz=current_hz;

    while (1) {
        mouse_poll(&ms);
        vesa_fill_screen(COLOR_BG);
        draw_desktop_icons();
        if (window_open==1) show_sysinfo_window();
        if (window_open==2) show_about_window();
        if (window_open==3) show_settings_window(ms.x,ms.y);
        if (window_open==4) show_hz_window(ms.x,ms.y);
        if (window_open==5||window_open==6) show_error_popup(ms.x,ms.y);
        draw_taskbar();
        if (start_open) draw_start_menu(ms.x,ms.y);
        mouse_draw_cursor(ms.x,ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;

        /* Hata popup */
        if (window_open==5||window_open==6) {
            int pw2=400,ph2=240;
            int px2=sw/2-pw2/2,py2=sh/2-ph2/2;
            if (ms.x>=px2+pw2-24&&ms.x<px2+pw2-8&&ms.y>=py2+8&&ms.y<py2+24)
                { window_open=(window_open==5)?3:4; continue; }
            int obx=px2+pw2/2-60,oby=py2+ph2-45;
            if (ms.x>=obx&&ms.x<obx+120&&ms.y>=oby&&ms.y<oby+32)
                { window_open=(window_open==5)?3:4; continue; }
            continue;
        }

        /* Pencere kapat */
        if (window_open==1&&ms.x>=160+480-24&&ms.x<160+480-8&&ms.y>=50+8&&ms.y<50+24)
            { window_open=0; continue; }
        if (window_open==2&&ms.x>=200+400-24&&ms.x<200+400-8&&ms.y>=120+8&&ms.y<120+24)
            { window_open=0; continue; }

        /* Renk ayarlari */
        if (window_open==3) {
            int wx3=130,wy3=30,ww3=540,wh3=510;
            if (ms.x>=wx3+ww3-24&&ms.x<wx3+ww3-8&&ms.y>=wy3+8&&ms.y<wy3+24)
                { window_open=0; continue; }
            /* Hz tab */
            if (ms.x>=wx3+180&&ms.x<wx3+340&&ms.y>=wy3+38&&ms.y<wy3+64)
                { window_open=4; selected_hz=current_hz; continue; }
            /* Butonlar */
            for (int i=0;i<9;i++) {
                int bx=wx3+15+(i%3)*175, by=wy3+142+(i/3)*52;
                if (ms.x>=bx&&ms.x<bx+165&&ms.y>=by&&ms.y<by+46)
                    { settings_selected_depth=dv[i]; break; }
            }
            /* Uygula */
            int abx=wx3+ww3/2-80, aby=wy3+wh3-42;
            if (ms.x>=abx&&ms.x<abx+160&&ms.y>=aby&&ms.y<aby+32) {
                int msup=0;
                for (int i=0;i<9;i++) if (dv[i]==settings_selected_depth) msup=mon_depth_sup[i];
                if (msup) {
                    vesa_set_depth(settings_selected_depth);
                    window_open=0;
                } else {
                    k_strcpy(error_title,"Hata - Renk Derinligi");
                    k_strcpy(error_line1,"Monitor bu renk derinligini");
                    char tmp[40]; k_strcpy(tmp,"desteklemiyor! Secilen: ");
                    char dd[6]; k_itoa(settings_selected_depth,dd,10);
                    int tl=k_strlen(tmp);
                    k_strcpy(tmp+tl,dd);
                    tl=k_strlen(tmp);
                    tmp[tl]='-'; tmp[tl+1]='b'; tmp[tl+2]='i'; tmp[tl+3]='t'; tmp[tl+4]='\0';
                    k_strcpy(error_line2,tmp);
                    char tmp2[48]; k_strcpy(tmp2,"Panel: ");
                    k_itoa(mon_max_bpp,tmp2+7,10);
                    int tl2=k_strlen(tmp2);
                    k_strcpy(tmp2+tl2,"-bit standart panel");
                    k_strcpy(error_line3,tmp2);
                    window_open=5;
                }
            }
            continue;
        }

        /* Hz ayarlari */
        if (window_open==4) {
            int wx3=130,wy3=30,ww3=540,wh3=510;
            if (ms.x>=wx3+ww3-24&&ms.x<wx3+ww3-8&&ms.y>=wy3+8&&ms.y<wy3+24)
                { window_open=0; continue; }
            /* Renk tab */
            if (ms.x>=wx3+10&&ms.x<wx3+170&&ms.y>=wy3+38&&ms.y<wy3+64)
                { window_open=3; settings_selected_depth=vesa_get_depth(); continue; }
            /* Butonlar */
            for (int i=0;i<9;i++) {
                int bx=wx3+15+(i%3)*175, by=wy3+152+(i/3)*55;
                if (ms.x>=bx&&ms.x<bx+165&&ms.y>=by&&ms.y<by+48)
                    { selected_hz=hz_vals[i]; break; }
            }
            /* Uygula */
            int abx=wx3+ww3/2-80, aby=wy3+wh3-42;
            if (ms.x>=abx&&ms.x<abx+160&&ms.y>=aby&&ms.y<aby+32) {
                int msup=0;
                for (int i=0;i<9;i++) if (hz_vals[i]==selected_hz) msup=mon_hz_sup[i];
                if (msup) {
                    current_hz=selected_hz;
                    window_open=0;
                } else {
                    k_strcpy(error_title,"Hata - Yenileme Hizi");
                    k_strcpy(error_line1,"Monitor bu yenileme hizini");
                    char tmp[40]; k_strcpy(tmp,"desteklemiyor! Secilen: ");
                    char dd[6]; k_itoa(selected_hz,dd,10);
                    int tl=k_strlen(tmp);
                    k_strcpy(tmp+tl,dd);
                    tl=k_strlen(tmp);
                    tmp[tl]=' '; tmp[tl+1]='H'; tmp[tl+2]='z'; tmp[tl+3]='\0';
                    k_strcpy(error_line2,tmp);
                    char tmp2[48]; k_strcpy(tmp2,"Monitor max: ");
                    k_itoa(mon_max_hz,tmp2+13,10);
                    int tl2=k_strlen(tmp2);
                    k_strcpy(tmp2+tl2," Hz");
                    k_strcpy(error_line3,tmp2);
                    window_open=6;
                }
            }
            continue;
        }

        /* Baslat */
        if (ms.x>=4&&ms.x<84&&ms.y>=sh-36&&ms.y<sh-4)
            { start_open=!start_open; window_open=0; continue; }

        /* Gear */
        if (ms.x>=sw-85&&ms.x<sw-55&&ms.y>=sh-32&&ms.y<sh-8)
            { window_open=3; settings_selected_depth=vesa_get_depth(); start_open=0; continue; }

        /* Menu */
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

        /* Ikonlar */
        for (int i=0;i<DICON_COUNT;i++) {
            int ix=dicons[i].x,iy=dicons[i].y;
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
