#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "io.h"
#include "browser.h"
#include "apps.h"
#include "net.h"
#include "task.h"
#include "heap.h"
#include "vfs.h"

#define COLOR_BG           0x1E1E2E
#define COLOR_TASKBAR      0x2D2D44
#define COLOR_TASKBAR_HOVER 0x3D3D5C
#define COLOR_START_BTN    0x0078D4
#define COLOR_START_HOVER  0x1A8AD4
#define COLOR_WINDOW_BG    0x2B2B3D
#define COLOR_WINDOW_TITLE 0x1F1F2F
#define COLOR_WINDOW_BORDER 0x444466
#define COLOR_TEXT_WHITE    0xFFFFFF
#define COLOR_TEXT_GREY     0xAAAAAA
#define COLOR_TEXT_BLUE     0x5599FF
#define COLOR_TEXT_GREEN    0x55FF55
#define COLOR_TEXT_YELLOW   0xFFFF55
#define COLOR_TEXT_RED      0xFF5555
#define COLOR_TEXT_CYAN     0x55FFFF
#define COLOR_BUTTON        0x3A3A5A
#define COLOR_BUTTON_HOVER  0x4A4A7A
#define COLOR_ACCENT        0x0078D4
#define COLOR_CLOSE_BTN     0xE81123
#define COLOR_MENU_BG       0x2B2B3D
#define COLOR_MENU_HOVER    0x0078D4
#define COLOR_DESKTOP_ICON  0x3A3A5A
#define COLOR_BLACK         0x000000

#define KEY_ESC 0x1B
#define KEY_F4  0x3C

static uint8_t bcd2bin(uint8_t b) { return ((b>>4)*10)+(b&0x0F); }

typedef struct { int x,y; const char* label; int id; } dicon_t;
static dicon_t dicons[] = {
    {30,40,"Bilgisayar",1}, {110,40,"Terminal",2}, {190,40,"Not Defteri",3},
    {270,40,"Hesap",4}, {350,40,"Tarayici",5}, {30,110,"Gorev Yon.",9}
};
#define DICON_COUNT 6

static int start_open=0, window_open=0;
static int dscroll=0;

/* =========== GOREV LISTESI ICIN TERMINAL =========== */
static void gt_puts_c(const char* s, int c) {
    (void)c;
    /* Basit terminal ciktisi - gercek terminalde renkli */
    vesa_draw_string(100, 400, s, COLOR_TEXT_WHITE, COLOR_BLACK);
}

/* =========== DESKTOP =========== */
static void draw_icons(int mx, int my) {
    for(int i=0;i<DICON_COUNT;i++) {
        int ix=dicons[i].x, iy=dicons[i].y-dscroll;
        if(iy<-30||iy>600) continue;
        
        int hover=(mx>=ix&&mx<ix+60&&my>=iy&&my<iy+50);
        uint32_t bg=hover?0x444466:COLOR_DESKTOP_ICON;
        
        if(dicons[i].id==1) {
            vesa_fill_rect(ix+2,iy+2,20,14,0x333355);
            vesa_fill_rect(ix+3,iy+3,18,12,COLOR_ACCENT);
            vesa_fill_rect(ix+9,iy+16,6,2,0x555577);
        }
        else if(dicons[i].id==2) {
            vesa_fill_rect(ix+1,iy+2,22,18,0x0C0C0C);
            vesa_draw_rect_outline(ix+1,iy+2,22,18,0x555577);
            vesa_fill_rect(ix+4,iy+8,2,2,COLOR_TEXT_GREEN);
            vesa_fill_rect(ix+6,iy+10,2,2,COLOR_TEXT_GREEN);
        }
        else if(dicons[i].id==9) {
            vesa_fill_rect(ix+2,iy+2,20,20,0x1A2A1A);
            vesa_draw_rect_outline(ix+2,iy+2,20,20,COLOR_TEXT_GREEN);
            vesa_fill_rect(ix+4,iy+5,16,3,COLOR_TEXT_GREEN);
            vesa_fill_rect(ix+4,iy+10,12,3,COLOR_TEXT_YELLOW);
            vesa_fill_rect(ix+4,iy+15,8,3,COLOR_TEXT_CYAN);
        }
        else {
            vesa_fill_rect(ix+2,iy+2,20,20,bg);
        }
        
        int tw=k_strlen(dicons[i].label)*8;
        vesa_draw_string_nobg(ix+12-tw/2,iy+28,dicons[i].label,COLOR_TEXT_WHITE);
    }
}

static void draw_taskbar(int mx, int my) {
    int sw=vesa_get_width(), sh=vesa_get_height();
    vesa_fill_rect(0,sh-40,sw,40,COLOR_TASKBAR);
    vesa_fill_rect(0,sh-40,sw,1,COLOR_WINDOW_BORDER);
    
    int start_hover=(mx>=4&&mx<84&&my>=sh-36&&my<sh-4);
    vesa_fill_rect(4,sh-36,80,32,start_hover?COLOR_START_HOVER:COLOR_START_BTN);
    vesa_fill_rect(12,sh-28,7,7,COLOR_TEXT_WHITE);
    vesa_fill_rect(21,sh-28,7,7,COLOR_TEXT_WHITE);
    vesa_fill_rect(12,sh-19,7,7,COLOR_TEXT_WHITE);
    vesa_fill_rect(21,sh-19,7,7,COLOR_TEXT_WHITE);
    vesa_draw_string(32,sh-26,"Baslat",COLOR_TEXT_WHITE,start_hover?COLOR_START_HOVER:COLOR_START_BTN);
    
    char clock[6];
    outb(0x70,0x04); uint8_t h=bcd2bin(inb(0x71));
    outb(0x70,0x02); uint8_t m=bcd2bin(inb(0x71));
    clock[0]='0'+h/10; clock[1]='0'+h%10; clock[2]=':';
    clock[3]='0'+m/10; clock[4]='0'+m%10; clock[5]=0;
    vesa_draw_string(sw-52,sh-26,clock,COLOR_TEXT_YELLOW,COLOR_TASKBAR);
    vesa_draw_string(sw/2-36,sh-26,"MyOS v0.5",COLOR_TEXT_GREY,COLOR_TASKBAR);
}

static void draw_start_menu(int mx, int my) {
    int sh=vesa_get_height();
    int smx=0,smy=sh-40-350,smw=240,smh=350;
    
    vesa_fill_rect(smx,smy,smw,smh,COLOR_MENU_BG);
    vesa_draw_rect_outline(smx,smy,smw,smh,COLOR_WINDOW_BORDER);
    
    vesa_fill_rect(smx+10,smy+10,smw-20,30,COLOR_WINDOW_TITLE);
    vesa_fill_rect(smx+18,smy+15,7,7,COLOR_ACCENT);
    vesa_fill_rect(smx+27,smy+15,7,7,COLOR_ACCENT);
    vesa_fill_rect(smx+18,smy+24,7,7,COLOR_ACCENT);
    vesa_fill_rect(smx+27,smy+24,7,7,COLOR_ACCENT);
    vesa_draw_string(smx+46,smy+17,"MyOS User",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    
    const char* items[]={
        "Terminal","Dosya Gezgini","Not Defteri","Hesap Makinesi",
        "Tarayici","Gorev Yoneticisi","Sistem Bilgisi","Hakkinda",
        "Yeniden Baslat","Kapat"
    };
    for(int i=0;i<10;i++) {
        int iy=smy+48+i*28;
        int hover=(mx>=smx&&mx<smx+smw&&my>=iy&&my<iy+28);
        uint32_t bg=hover?COLOR_MENU_HOVER:COLOR_MENU_BG;
        vesa_fill_rect(smx+5,iy,smw-10,28,bg);
        vesa_draw_string(smx+15,iy+6,items[i],COLOR_TEXT_WHITE,bg);
    }
}

/* =========== PENCERELER =========== */
static void show_sysinfo(void) {
    int wx=180,wy=80,ww=440,wh=350;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Sistem Bilgisi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    
    char vendor[13];
    uint32_t ebx,ecx,edx;
    __asm__ __volatile__("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    *((uint32_t*)&vendor[0])=ebx; *((uint32_t*)&vendor[4])=edx; *((uint32_t*)&vendor[8])=ecx;
    vendor[12]=0;
    
    int cy=wy+50;
    vesa_draw_string(wx+60,cy,"CPU:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+100,cy,vendor,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Mimari:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+130,cy,"i686 32-bit",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=30;
    char res[20]; k_itoa(vesa_get_width(),res,10); int l=k_strlen(res);
    res[l]='x'; k_itoa(vesa_get_height(),res+l+1,10);
    vesa_draw_string(wx+60,cy,"Ekran:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+130,cy,res,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Versiyon:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+130,cy,"MyOS v0.5",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    cy+=30;
    vesa_draw_string(wx+60,cy,"Gorev Sayisi:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    char tc[4]; k_itoa(task_get_count(),tc,10);
    vesa_draw_string(wx+180,cy,tc,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    
    /* Kapat butonu */
    vesa_fill_rect(wx+ww-28,wy+4,24,24,COLOR_CLOSE_BTN);
    vesa_draw_string(wx+ww-20,wy+8,"X",COLOR_TEXT_WHITE,COLOR_CLOSE_BTN);
}

static void show_about(void) {
    int wx=200,wy=120,ww=400,wh=260;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Hakkinda",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    
    vesa_fill_rect(wx+ww/2-8,wy+50,16,16,COLOR_ACCENT);
    vesa_fill_rect(wx+ww/2-8+9,wy+50,7,7,COLOR_ACCENT);
    vesa_fill_rect(wx+ww/2-8,wy+50+9,7,7,COLOR_ACCENT);
    vesa_fill_rect(wx+ww/2-8+9,wy+50+9,7,7,COLOR_ACCENT);
    
    vesa_draw_string(wx+ww/2-40,wy+80,"MyOS",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2-60,wy+100,"Versiyon 0.5.0",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+140,"Sifirdan yazilmis isletim sistemi",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+170,"x86 32-bit | VESA | Multitasking",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    
    vesa_fill_rect(wx+ww-28,wy+4,24,24,COLOR_CLOSE_BTN);
    vesa_draw_string(wx+ww-20,wy+8,"X",COLOR_TEXT_WHITE,COLOR_CLOSE_BTN);
}

/* =========== DESKTOP =========== */
static void draw_desktop(int mx, int my) {
    vesa_fill_screen(COLOR_BG);
    draw_icons(mx,my);
    if(window_open==1) show_sysinfo();
    if(window_open==2) show_about();
    draw_taskbar(mx,my);
    if(start_open) draw_start_menu(mx,my);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);
    vesa_draw_string(320,290,"Kapatiliyor...",COLOR_TEXT_WHITE,COLOR_BLACK);
    vesa_copy_buffer();
    outb(0x604,0x2000); outb(0xB004,0x2000); outb(0x4004,0x3400);
    __asm__ __volatile__("cli;hlt");
}

/* =========== TERMINAL ICIN TASKS KOMUTU =========== */
static int gt_process(const char* c) {
    if(!k_strcmp(c,"help")) {
        gt_puts_c("=== MyOS Komutlari ===\n",0);
        gt_puts_c("help     - Yardim\n",0);
        gt_puts_c("clear    - Ekrani temizle\n",0);
        gt_puts_c("meminfo  - Bellek bilgisi\n",0);
        gt_puts_c("tasks    - Gorev listesi\n",0);
        gt_puts_c("reboot   - Yeniden baslat\n",0);
        return 0;
    }
    if(!k_strcmp(c,"clear")) {
        vesa_fill_rect(100,100,600,400,COLOR_BLACK);
        return 0;
    }
    if(!k_strcmp(c,"meminfo")) {
        char b[32];
        gt_puts_c("Toplam RAM: ",0); k_itoa((int)ram_get_total_kb(),b,10); gt_puts_c(b,0); gt_puts_c(" KB\n",0);
        gt_puts_c("Heap kullanim: ",0); k_itoa((int)heap_get_used(),b,10); gt_puts_c(b,0); gt_puts_c(" B\n",0);
        return 0;
    }
    if(!k_strcmp(c,"tasks")) {
        gt_puts_c("=== Aktif Gorevler ===\n",0);
        int cnt=task_get_count();
        char buf[64];
        k_itoa(cnt,buf,10);
        gt_puts_c("Toplam: ",0); gt_puts_c(buf,0); gt_puts_c("\n",0);
        
        for(int i=0;i<MAX_TASKS;i++) {
            task_t* t=task_get(i);
            if(!t||t->state==TASK_STATE_FREE) continue;
            
            k_itoa(t->id,buf,10);
            gt_puts_c("[",0); gt_puts_c(buf,0); gt_puts_c("] ",0);
            gt_puts_c(t->name,0);
            gt_puts_c(" - ",0);
            
            const char* st;
            switch(t->state) {
                case TASK_STATE_RUNNING: st="RUN"; break;
                case TASK_STATE_READY: st="RDY"; break;
                case TASK_STATE_SLEEPING: st="SLP"; break;
                default: st="???"; break;
            }
            gt_puts_c(st,0);
            gt_puts_c(" cpu=",0);
            k_itoa((int)t->cpu_time,buf,10);
            gt_puts_c(buf,0);
            gt_puts_c("\n",0);
        }
        return 0;
    }
    if(!k_strcmp(c,"reboot")) {
        outb(0x64,0xFE);
        return 0;
    }
    return -1;
}

/* =========== ANA DONGU =========== */
void gui_run(void) {
    mouse_state_t ms;
    
    /* Arka plan gorevleri */
    task_create("net_poll",0,TASK_PRIORITY_SYSTEM);
    
    while(1) {
        mouse_poll(&ms);
        int key=keyboard_poll();
        
        if(key==KEY_F4&&keyboard_alt_held()) do_shutdown();
        
        draw_desktop(ms.x,ms.y);
        mouse_draw_cursor(ms.x,ms.y);
        vesa_copy_buffer();
        
        net_poll();
        task_schedule();
        
        if(!ms.click) continue;
        
        /* Pencere kapatma */
        if(window_open) {
            int wx=(window_open==1)?180:200;
            int wy=80,ww=(window_open==1)?440:400;
            if(ms.x>=wx+ww-28&&ms.x<wx+ww-4&&ms.y>=wy+4&&ms.y<wy+28) {
                window_open=0; continue;
            }
        }
        
        /* Baslat butonu */
        if(ms.x>=4&&ms.x<84&&ms.y>=vesa_get_height()-36&&ms.y<vesa_get_height()-4) {
            start_open=!start_open; continue;
        }
        
        /* Baslat menu */
        if(start_open) {
            int sh=vesa_get_height();
            int smy=sh-40-350;
            for(int i=0;i<10;i++) {
                int iy=smy+48+i*28;
                if(ms.x>=0&&ms.x<240&&ms.y>=iy&&ms.y<iy+28) {
                    start_open=0;
                    if(i==0) app_file_explorer();
                    else if(i==1) app_file_explorer();
                    else if(i==2) app_notepad();
                    else if(i==3) app_calculator();
                    else if(i==4) app_browser();
                    else if(i==5) app_task_manager();
                    else if(i==6) window_open=1;
                    else if(i==7) window_open=2;
                    else if(i==8) outb(0x64,0xFE);
                    else if(i==9) do_shutdown();
                    break;
                }
            }
            if(ms.x>240||ms.y<smy) start_open=0;
            continue;
        }
        
        /* Desktop ikonlari */
        for(int i=0;i<DICON_COUNT;i++) {
            int ix=dicons[i].x, iy=dicons[i].y-dscroll;
            if(ms.x>=ix&&ms.x<ix+60&&ms.y>=iy&&ms.y<iy+50) {
                if(dicons[i].id==1) window_open=1;
                else if(dicons[i].id==2) app_file_explorer();
                else if(dicons[i].id==3) app_notepad();
                else if(dicons[i].id==4) app_calculator();
                else if(dicons[i].id==5) app_browser();
                else if(dicons[i].id==9) app_task_manager();
                break;
            }
        }
    }
}
