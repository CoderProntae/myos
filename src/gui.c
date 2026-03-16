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
static int current_hz = 60;
static int selected_hz = 60;

static const int dv[9]={1,4,8,16,24,30,32,36,48};
static const int hv[9]={30,50,60,75,100,120,144,165,240};

static uint8_t bcd2bin(uint8_t b){return((b>>4)*10)+(b&0x0F);}
static void get_time(char*buf){
    outb(0x70,0x04);uint8_t h=bcd2bin(inb(0x71));
    outb(0x70,0x02);uint8_t m=bcd2bin(inb(0x71));
    buf[0]='0'+h/10;buf[1]='0'+h%10;buf[2]=':';
    buf[3]='0'+m/10;buf[4]='0'+m%10;buf[5]='\0';
}

static const char* depth_info(int d){
    switch(d){case 1:return"2 renk";case 4:return"64 renk";case 8:return"256 renk";
    case 16:return"65K renk";case 24:return"16.7M renk";case 30:return"1.07 Milyar";
    case 32:return"16.7M+Alpha";case 36:return"68.7 Milyar";case 48:return"281 Trilyon";
    default:return"";}
}
static const char* hz_info(int h){
    switch(h){case 30:return"Sinema";case 50:return"PAL";case 60:return"Standart";
    case 75:return"Gelismis";case 100:return"Yuksek";case 120:return"Oyun";
    case 144:return"Gaming";case 165:return"Gaming+";case 240:return"E-spor";
    default:return"";}
}

/* ======== TERMINAL ======== */
#define GT_COLS 88
#define GT_ROWS 30
static char gt_buf[GT_ROWS][GT_COLS+1];
static int gt_cx,gt_cy;
static char gt_cmd[256];
static int gt_cmd_len;
static void gt_clear(void){for(int r=0;r<GT_ROWS;r++)k_memset(gt_buf[r],0,GT_COLS+1);gt_cx=gt_cy=0;}
static void gt_scroll(void){while(gt_cy>=GT_ROWS){for(int r=0;r<GT_ROWS-1;r++)for(int c=0;c<=GT_COLS;c++)gt_buf[r][c]=gt_buf[r+1][c];k_memset(gt_buf[GT_ROWS-1],0,GT_COLS+1);gt_cy--;}}
static void gt_putc(char c){if(c=='\n'){gt_cx=0;gt_cy++;gt_scroll();}else if(c=='\b'){if(gt_cx>0){gt_cx--;gt_buf[gt_cy][gt_cx]=' ';}}else if(gt_cx<GT_COLS){gt_buf[gt_cy][gt_cx]=c;gt_cx++;if(gt_cx>=GT_COLS){gt_cx=0;gt_cy++;gt_scroll();}}}
static void gt_puts(const char*s){while(*s)gt_putc(*s++);}
static void gt_render(void){
    int sw2=vesa_get_width(),sh2=vesa_get_height();
    int wx=20,wy=10,ww=sw2-40,wh=sh2-50;
    vesa_fill_screen(COLOR_BG);
    vesa_fill_rect(wx,wy,ww,wh,COLOR_TERMINAL_BG);
    vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Terminal - MyOS",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    for(int r=0;r<GT_ROWS;r++)for(int c=0;c<GT_COLS;c++){char ch=gt_buf[r][c];if(ch&&ch!=' ')vesa_draw_char(28+c*8,50+r*16,ch,c<8?COLOR_TEXT_GREEN:COLOR_TEXT_GREY,COLOR_TERMINAL_BG);}
    vesa_fill_rect(28+gt_cx*8,50+gt_cy*16,8,16,0x888888);
    vesa_copy_buffer();
}
static void gt_prompt(void){gt_puts("myos:~$ ");gt_cmd_len=0;k_memset(gt_cmd,0,256);}
static int gt_process(void){
    gt_putc('\n');const char*c=gt_cmd;while(*c==' ')c++;
    if(!k_strlen(c))return 0;
    if(!k_strcmp(c,"exit"))return 1;
    if(!k_strcmp(c,"clear")){gt_clear();return 0;}
    if(!k_strcmp(c,"help")){gt_puts("  help clear echo time sysinfo exit\n");return 0;}
    if(!k_strcmp(c,"time")){outb(0x70,4);uint8_t h=bcd2bin(inb(0x71));outb(0x70,2);uint8_t m=bcd2bin(inb(0x71));outb(0x70,0);uint8_t s=bcd2bin(inb(0x71));char t[12];t[0]=' ';t[1]=' ';t[2]='0'+h/10;t[3]='0'+h%10;t[4]=':';t[5]='0'+m/10;t[6]='0'+m%10;t[7]=':';t[8]='0'+s/10;t[9]='0'+s%10;t[10]='\n';t[11]=0;gt_puts(t);return 0;}
    if(!k_strcmp(c,"sysinfo")){
        char v[13];uint32_t eb,ec,ed;__asm__ __volatile__("cpuid":"=b"(eb),"=c"(ec),"=d"(ed):"a"(0));
        *((uint32_t*)&v[0])=eb;*((uint32_t*)&v[4])=ed;*((uint32_t*)&v[8])=ec;v[12]=0;
        monitor_info_t*mi=vesa_get_monitor_info();
        gt_puts("  CPU      : ");gt_puts(v);gt_putc('\n');
        gt_puts("  Adaptor  : ");gt_puts(mi->adapter_name);gt_putc('\n');
        gt_puts("  Monitor  : ");gt_puts(mi->monitor_name);gt_putc('\n');
        char vb[12];k_itoa((int)(mi->vram_bytes/1024/1024),vb,10);
        gt_puts("  VRAM     : ");gt_puts(vb);gt_puts(" MB\n");
        gt_puts("  Ortam    : ");gt_puts(mi->is_virtual?"Sanal Makine":"Fiziksel\n");gt_putc('\n');
        char db[10];k_itoa(vesa_get_depth(),db,10);
        gt_puts("  Renk     : ");gt_puts(db);gt_puts("-bit\n");
        char hb[10];k_itoa(current_hz,hb,10);
        gt_puts("  Hz       : ");gt_puts(hb);gt_puts(" Hz\n");
        return 0;}
    if(!k_strncmp(c,"echo ",5)){gt_puts("  ");gt_puts(c+5);gt_putc('\n');return 0;}
    if(!k_strcmp(c,"reboot")){outb(0x64,0xFE);return 0;}
    gt_puts("  Bilinmeyen: ");gt_puts(c);gt_puts("\n");return 0;
}
static void gui_terminal(void){
    gt_clear();gt_puts("MyOS Terminal v0.3\n'exit' ile cik.\n\n");gt_prompt();gt_render();
    while(1){char c=keyboard_getchar();if(c=='\n'){gt_cmd[gt_cmd_len]=0;if(gt_process())return;gt_prompt();}else if(c=='\b'){if(gt_cmd_len>0){gt_cmd_len--;gt_cmd[gt_cmd_len]=0;gt_putc('\b');}}else if(c>=32&&c<127&&gt_cmd_len<250){gt_cmd[gt_cmd_len++]=c;gt_putc(c);}gt_render();}
}

/* ======== MASAUSTU ======== */
typedef struct{int x,y;const char*label;int id;}dicon_t;
static dicon_t dicons[]={{30,40,"Bilgisayarim",1},{30,120,"Terminal",2},{30,200,"Hakkinda",3},{30,280,"Ekran Ayar",4}};
#define DICON_COUNT 4

static void draw_icons(void){for(int i=0;i<DICON_COUNT;i++){int ix=dicons[i].x,iy=dicons[i].y;if(dicons[i].id==1)draw_icon_computer(ix,iy);else if(dicons[i].id==2)draw_icon_terminal(ix,iy);else if(dicons[i].id==3)draw_icon_info(ix,iy);else draw_icon_display(ix,iy);int tw=k_strlen(dicons[i].label)*8;vesa_draw_string_nobg(ix+12-tw/2,iy+28,dicons[i].label,COLOR_TEXT_WHITE);}}

static void draw_taskbar(void){
    int sw2=vesa_get_width(),sh2=vesa_get_height(),ty=sh2-40;
    vesa_fill_rect(0,ty,sw2,40,COLOR_TASKBAR);
    vesa_fill_rect(0,ty,sw2,1,COLOR_WINDOW_BORDER);
    uint32_t sc=start_open?COLOR_START_HOVER:COLOR_START_BTN;
    vesa_fill_rect(4,ty+4,80,32,sc);
    draw_windows_logo(12,ty+10,COLOR_TEXT_WHITE);
    vesa_draw_string(32,ty+12,"Baslat",COLOR_TEXT_WHITE,sc);
    char ck[6];get_time(ck);
    vesa_draw_string(sw2-48,ty+12,ck,COLOR_TEXT_WHITE,COLOR_TASKBAR);
    draw_icon_gear(sw2-75,ty+12,COLOR_TEXT_GREY);
    char dbuf[16];k_itoa(vesa_get_depth(),dbuf,10);int dl=k_strlen(dbuf);dbuf[dl]='b';dbuf[dl+1]=' ';dbuf[dl+2]=0;
    int dl2=k_strlen(dbuf);k_itoa(current_hz,dbuf+dl2,10);int dl3=k_strlen(dbuf);dbuf[dl3]='H';dbuf[dl3+1]='z';dbuf[dl3+2]=0;
    vesa_draw_string(sw2-145,ty+12,dbuf,COLOR_TEXT_CYAN,COLOR_TASKBAR);
    vesa_draw_string(sw2/2-36,ty+12,"MyOS v0.3",COLOR_TEXT_GREY,COLOR_TASKBAR);
}

static void draw_start_menu(int mx2,int my2){
    int sh2=vesa_get_height(),smy=sh2-40-220;
    vesa_fill_rect(0,smy,240,220,COLOR_MENU_BG);vesa_draw_rect_outline(0,smy,240,220,COLOR_WINDOW_BORDER);
    vesa_fill_rect(10,smy+10,220,30,COLOR_WINDOW_TITLE);draw_windows_logo(18,smy+15,COLOR_ACCENT);
    vesa_draw_string(46,smy+17,"MyOS User",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    const char*items[]={"Terminal","Sistem Bilgisi","Hakkinda","Yeniden Baslat","Kapat"};
    for(int i=0;i<5;i++){int iy=smy+50+i*32;int h=(mx2>=0&&mx2<240&&my2>=iy&&my2<iy+32);
    vesa_fill_rect(1,iy,238,32,h?COLOR_MENU_HOVER:COLOR_MENU_BG);
    vesa_draw_string(20,iy+8,items[i],COLOR_TEXT_WHITE,h?COLOR_MENU_HOVER:COLOR_MENU_BG);
    if(i==4)draw_icon_power(206,iy+4);}
}

static void show_sysinfo(void){
    monitor_info_t*mi=vesa_get_monitor_info();
    int wx=150,wy=40,ww=500,wh=460;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Sistem Bilgisi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);
    char v[13];uint32_t eb,ec,ed;__asm__ __volatile__("cpuid":"=b"(eb),"=c"(ec),"=d"(ed):"a"(0));
    *((uint32_t*)&v[0])=eb;*((uint32_t*)&v[4])=ed;*((uint32_t*)&v[8])=ec;v[12]=0;
    int cy=wy+48;
    vesa_draw_string(wx+20,cy,"CPU:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,v,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);cy+=22;
    vesa_draw_string(wx+20,cy,"Adaptor:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,mi->adapter_name,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);cy+=22;
    vesa_draw_string(wx+20,cy,"Monitor:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,mi->monitor_name,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);cy+=22;
    vesa_draw_string(wx+20,cy,"Ortam:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,mi->is_virtual?"Sanal Makine (VM)":"Fiziksel Donanim",mi->is_virtual?COLOR_TEXT_YELLOW:COLOR_TEXT_GREEN,COLOR_WINDOW_BG);cy+=22;
    char vb[12];k_itoa((int)(mi->vram_bytes/1024/1024),vb,10);int vl=k_strlen(vb);vb[vl]=' ';vb[vl+1]='M';vb[vl+2]='B';vb[vl+3]=0;
    vesa_draw_string(wx+20,cy,"VRAM:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,vb,COLOR_TEXT_CYAN,COLOR_WINDOW_BG);cy+=22;
    char rs[20];k_itoa(vesa_get_width(),rs,10);int rl=k_strlen(rs);rs[rl]='x';k_itoa(vesa_get_height(),rs+rl+1,10);
    vesa_draw_string(wx+20,cy,"Cozunurluk:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,rs,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);cy+=22;
    char db[10];k_itoa(vesa_get_depth(),db,10);int dl=k_strlen(db);db[dl]='-';db[dl+1]='b';db[dl+2]='i';db[dl+3]='t';db[dl+4]=0;
    vesa_draw_string(wx+20,cy,"Renk:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,db,COLOR_TEXT_CYAN,COLOR_WINDOW_BG);cy+=22;
    char hb[10];k_itoa(current_hz,hb,10);int hl=k_strlen(hb);hb[hl]=' ';hb[hl+1]='H';hb[hl+2]='z';hb[hl+3]=0;
    vesa_draw_string(wx+20,cy,"Yenileme:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,hb,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);cy+=22;
    char mhz[10];k_itoa(mi->max_hz,mhz,10);int ml=k_strlen(mhz);mhz[ml]=' ';mhz[ml+1]='H';mhz[ml+2]='z';mhz[ml+3]=0;
    vesa_draw_string(wx+20,cy,"Max Hz:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,mhz,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);cy+=22;
    char mb[10];k_itoa(mi->max_bpp,mb,10);int mbl=k_strlen(mb);mb[mbl]='-';mb[mbl+1]='b';mb[mbl+2]='i';mb[mbl+3]='t';mb[mbl+4]=0;
    vesa_draw_string(wx+20,cy,"Max BPP:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,mb,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);cy+=22;
    /* Adaptor ID */
    char aid[8];k_itoa(mi->adapter_id,aid,16);
    vesa_draw_string(wx+20,cy,"VBE ID:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+130,cy,"0x",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+146,cy,aid,COLOR_TEXT_GREY,COLOR_WINDOW_BG);
}

static void show_about(void){
    int wx=200,wy=120,ww=400,wh=260;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Hakkinda",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);
    draw_windows_logo(wx+ww/2-8,wy+50,COLOR_ACCENT);
    vesa_draw_string(wx+ww/2-28,wy+80,"MyOS",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2-52,wy+100,"Versiyon 0.3.0",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+140,"Sifirdan yazilmis isletim sistemi",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+170,"x86 32-bit | VESA Grafik",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+200,"Gercek donanim algilama aktif",COLOR_TEXT_GREEN,COLOR_WINDOW_BG);
}

/* ======== AYARLAR ======== */
static void show_depth_settings(int mx2,int my2){
    monitor_info_t*mi=vesa_get_monitor_info();
    int wx=130,wy=30,ww=540,wh=510;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran - Renk Derinligi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);
    /* Tablar */
    vesa_fill_rect(wx+10,wy+38,160,26,COLOR_ACCENT);
    vesa_draw_string(wx+20,wy+43,"Renk Derinligi",COLOR_TEXT_WHITE,COLOR_ACCENT);
    int ht=(mx2>=wx+180&&mx2<wx+340&&my2>=wy+38&&my2<wy+64);
    vesa_fill_rect(wx+180,wy+38,160,26,ht?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    vesa_draw_string(wx+194,wy+43,"Yenileme Hizi >>",COLOR_TEXT_WHITE,ht?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    /* Monitor kutusu */
    vesa_fill_rect(wx+15,wy+70,ww-30,45,0x1A1A2A);vesa_draw_rect_outline(wx+15,wy+70,ww-30,45,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+25,wy+75,"Adaptor:",COLOR_TEXT_GREY,0x1A1A2A);vesa_draw_string(wx+110,wy+75,mi->adapter_name,COLOR_TEXT_WHITE,0x1A1A2A);
    char mb[16];k_itoa(mi->max_bpp,mb,10);int mbl=k_strlen(mb);mb[mbl]='-';mb[mbl+1]='b';mb[mbl+2]='i';mb[mbl+3]='t';mb[mbl+4]=0;
    vesa_draw_string(wx+25,wy+93,"Panel:",COLOR_TEXT_GREY,0x1A1A2A);vesa_draw_string(wx+110,wy+93,mb,COLOR_TEXT_GREEN,0x1A1A2A);
    /* Butonlar */
    for(int i=0;i<9;i++){
        int bx=wx+15+(i%3)*175,by=wy+122+(i/3)*52,bw=165,bh=46;
        int hov=(mx2>=bx&&mx2<bx+bw&&my2>=by&&my2<by+bh);
        int sel=(dv[i]==settings_selected_depth);int act=(dv[i]==vesa_get_depth());
        uint32_t bg=sel?COLOR_ACCENT:hov?COLOR_BUTTON_HOVER:COLOR_BUTTON;
        vesa_fill_rect(bx,by,bw,bh,bg);vesa_draw_rect_outline(bx,by,bw,bh,sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if(act)vesa_draw_rect_outline(bx+1,by+1,bw-2,bh-2,COLOR_TEXT_GREEN);
        char lb[12];k_itoa(dv[i],lb,10);int ll=k_strlen(lb);lb[ll]='-';lb[ll+1]='b';lb[ll+2]='i';lb[ll+3]='t';lb[ll+4]=0;
        vesa_draw_string(bx+8,by+4,lb,COLOR_TEXT_WHITE,bg);
        vesa_draw_string(bx+70,by+4,"OS:",COLOR_TEXT_GREY,bg);vesa_fill_rect(bx+96,by+6,8,8,COLOR_TEXT_GREEN);
        vesa_draw_string(bx+8,by+24,"Mon:",COLOR_TEXT_GREY,bg);
        if(mi->sup_depth[i]){vesa_fill_rect(bx+44,by+26,8,8,COLOR_TEXT_GREEN);vesa_draw_string(bx+56,by+24,depth_info(dv[i]),COLOR_TEXT_GREY,bg);}
        else{vesa_fill_rect(bx+44,by+26,8,8,COLOR_TEXT_RED);vesa_draw_string(bx+56,by+24,"Panel yetersiz",0xFF8844,bg);}
    }
    /* Onizleme */
    int gy=wy+285;char st[12];k_itoa(settings_selected_depth,st,10);int sl=k_strlen(st);st[sl]='-';st[sl+1]='b';st[sl+2]='i';st[sl+3]='t';st[sl+4]=0;
    vesa_draw_string(wx+15,gy,"Onizleme:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+95,gy,st,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    gy+=18;int gw=ww-30;
    for(int px=0;px<gw;px++){uint32_t r=(uint32_t)((px*255)/gw),g=255-r,b=(uint32_t)((px*128)/gw)+64;
    uint32_t pc=vesa_preview_color((r<<16)|(g<<8)|b,settings_selected_depth);
    for(int py=0;py<14;py++)vesa_putpixel_raw(wx+15+px,gy+py,pc);}
    uint32_t tc[]={0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0x0078D4,0xFFFFFF};
    for(int i=0;i<8;i++){uint32_t c=vesa_preview_color(tc[i],settings_selected_depth);int bxx=wx+15+i*(gw/8);
    for(int py=0;py<14;py++)for(int px=0;px<gw/8-2;px++)vesa_putpixel_raw(bxx+px,gy+16+py,c);}
    vesa_draw_rect_outline(wx+15,gy,gw,14,COLOR_WINDOW_BORDER);vesa_draw_rect_outline(wx+15,gy+16,gw,14,COLOR_WINDOW_BORDER);
    /* Durum */
    int msup=0;for(int i=0;i<9;i++)if(dv[i]==settings_selected_depth)msup=mi->sup_depth[i];
    int sy=gy+36;
    if(msup){vesa_fill_rect(wx+15,sy,ww-30,20,0x1A2A1A);vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x228822);vesa_draw_string(wx+25,sy+2,"Monitor destekliyor",COLOR_TEXT_GREEN,0x1A2A1A);}
    else{vesa_fill_rect(wx+15,sy,ww-30,20,0x3A1A1A);vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x882222);vesa_draw_string(wx+25,sy+2,"! Monitor desteklemiyor - Uygulayinca hata verir",COLOR_TEXT_RED,0x3A1A1A);}
    int abx=wx+ww/2-80,aby=wy+wh-42;int ah=(mx2>=abx&&mx2<abx+160&&my2>=aby&&my2<aby+32);
    vesa_fill_rect(abx,aby,160,32,ah?COLOR_START_HOVER:COLOR_ACCENT);vesa_draw_rect_outline(abx,aby,160,32,0x005599);
    vesa_draw_string(abx+44,aby+8,"Uygula",COLOR_TEXT_WHITE,ah?COLOR_START_HOVER:COLOR_ACCENT);
}

static void show_hz_settings(int mx2,int my2){
    monitor_info_t*mi=vesa_get_monitor_info();
    int wx=130,wy=30,ww=540,wh=510;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    draw_icon_gear(wx+8,wy+8,COLOR_TEXT_WHITE);
    vesa_draw_string(wx+30,wy+8,"Ekran - Yenileme Hizi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);
    int ct=(mx2>=wx+10&&mx2<wx+170&&my2>=wy+38&&my2<wy+64);
    vesa_fill_rect(wx+10,wy+38,160,26,ct?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    vesa_draw_string(wx+16,wy+43,"<< Renk Derinligi",COLOR_TEXT_WHITE,ct?COLOR_BUTTON_HOVER:COLOR_BUTTON);
    vesa_fill_rect(wx+180,wy+38,160,26,COLOR_ACCENT);
    vesa_draw_string(wx+196,wy+43,"Yenileme Hizi",COLOR_TEXT_WHITE,COLOR_ACCENT);
    /* Monitor */
    vesa_fill_rect(wx+15,wy+70,ww-30,55,0x1A1A2A);vesa_draw_rect_outline(wx+15,wy+70,ww-30,55,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+25,wy+77,"Monitor:",COLOR_TEXT_GREY,0x1A1A2A);vesa_draw_string(wx+120,wy+77,mi->monitor_name,COLOR_TEXT_WHITE,0x1A1A2A);
    char mhz[16];k_itoa(mi->max_hz,mhz,10);int ml=k_strlen(mhz);mhz[ml]=' ';mhz[ml+1]='H';mhz[ml+2]='z';mhz[ml+3]=' ';mhz[ml+4]='m';mhz[ml+5]='a';mhz[ml+6]='x';mhz[ml+7]=0;
    vesa_draw_string(wx+25,wy+97,"Algilanan max:",COLOR_TEXT_GREY,0x1A1A2A);vesa_draw_string(wx+150,wy+97,mhz,COLOR_TEXT_GREEN,0x1A1A2A);
    /* Butonlar */
    for(int i=0;i<9;i++){
        int bx=wx+15+(i%3)*175,by=wy+132+(i/3)*55,bw=165,bh=48;
        int hov=(mx2>=bx&&mx2<bx+bw&&my2>=by&&my2<by+bh);
        int sel=(hv[i]==selected_hz);int act=(hv[i]==current_hz);
        uint32_t bg=sel?COLOR_ACCENT:hov?COLOR_BUTTON_HOVER:COLOR_BUTTON;
        vesa_fill_rect(bx,by,bw,bh,bg);vesa_draw_rect_outline(bx,by,bw,bh,sel?0x44AAFF:COLOR_WINDOW_BORDER);
        if(act)vesa_draw_rect_outline(bx+1,by+1,bw-2,bh-2,COLOR_TEXT_GREEN);
        char lb[12];k_itoa(hv[i],lb,10);int ll=k_strlen(lb);lb[ll]=' ';lb[ll+1]='H';lb[ll+2]='z';lb[ll+3]=0;
        vesa_draw_string(bx+8,by+4,lb,COLOR_TEXT_WHITE,bg);
        vesa_draw_string(bx+70,by+4,"OS:",COLOR_TEXT_GREY,bg);vesa_fill_rect(bx+96,by+6,8,8,COLOR_TEXT_GREEN);
        vesa_draw_string(bx+8,by+26,"Mon:",COLOR_TEXT_GREY,bg);
        if(mi->sup_hz[i]){vesa_fill_rect(bx+44,by+28,8,8,COLOR_TEXT_GREEN);vesa_draw_string(bx+56,by+26,hz_info(hv[i]),COLOR_TEXT_GREY,bg);}
        else{vesa_fill_rect(bx+44,by+28,8,8,COLOR_TEXT_RED);vesa_draw_string(bx+56,by+26,"Monitor yetersiz",0xFF8844,bg);}
    }
    /* Durum */
    int msup=0;for(int i=0;i<9;i++)if(hv[i]==selected_hz)msup=mi->sup_hz[i];
    int sy=wy+305;
    if(msup){vesa_fill_rect(wx+15,sy,ww-30,20,0x1A2A1A);vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x228822);vesa_draw_string(wx+25,sy+2,"Monitor destekliyor",COLOR_TEXT_GREEN,0x1A2A1A);}
    else{vesa_fill_rect(wx+15,sy,ww-30,20,0x3A1A1A);vesa_draw_rect_outline(wx+15,sy,ww-30,20,0x882222);vesa_draw_string(wx+25,sy+2,"! Monitor desteklemiyor - Uygulayinca hata verir",COLOR_TEXT_RED,0x3A1A1A);}
    int abx=wx+ww/2-80,aby=wy+wh-42;int ah=(mx2>=abx&&mx2<abx+160&&my2>=aby&&my2<aby+32);
    vesa_fill_rect(abx,aby,160,32,ah?COLOR_START_HOVER:COLOR_ACCENT);vesa_draw_rect_outline(abx,aby,160,32,0x005599);
    vesa_draw_string(abx+44,aby+8,"Uygula",COLOR_TEXT_WHITE,ah?COLOR_START_HOVER:COLOR_ACCENT);
}

/* ======== HATA POPUP ======== */
static char err_title[32],err_l1[64],err_l2[64],err_l3[64];

static void show_error(int mx2,int my2){
    int pw=400,ph=240,px=vesa_get_width()/2-pw/2,py=vesa_get_height()/2-ph/2;
    /* Karart */
    uint32_t*bb=backbuffer;int sw2=vesa_get_width(),sh2=vesa_get_height();
    for(int y=0;y<sh2;y++)for(int x=0;x<sw2;x++)if(x<px||x>=px+pw||y<py||y>=py+ph){uint32_t o=bb[y*sw2+x];bb[y*sw2+x]=((o>>16&0xFF)/3<<16)|((o>>8&0xFF)/3<<8)|(o&0xFF)/3;}
    vesa_fill_rect(px,py,pw,ph,COLOR_WINDOW_BG);vesa_fill_rect(px,py,pw,32,0x881122);vesa_draw_rect_outline(px,py,pw,ph,0xAA2233);
    vesa_draw_string(px+16,py+8,err_title,COLOR_TEXT_WHITE,0x881122);draw_close_button(px+pw-24,py+8,COLOR_CLOSE_BTN);
    /* ! ikon */
    int icx=px+pw/2,icy=py+72;
    for(int dy=-18;dy<=18;dy++)for(int dx=-18;dx<=18;dx++){int d=dx*dx+dy*dy;if(d<=324&&d>=256)vesa_putpixel_raw(icx+dx,icy+dy,COLOR_TEXT_RED);}
    vesa_fill_rect(icx-2,icy-10,4,12,COLOR_TEXT_RED);vesa_fill_rect(icx-2,icy+5,4,4,COLOR_TEXT_RED);
    vesa_draw_string(px+30,py+105,err_l1,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(px+30,py+125,err_l2,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    vesa_draw_string(px+30,py+145,err_l3,COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    int obx=px+pw/2-60,oby=py+ph-45;int oh=(mx2>=obx&&mx2<obx+120&&my2>=oby&&oby<oby+32);
    vesa_fill_rect(obx,oby,120,32,oh?COLOR_START_HOVER:COLOR_ACCENT);vesa_draw_rect_outline(obx,oby,120,32,0x005599);
    vesa_draw_string(obx+36,oby+8,"Tamam",COLOR_TEXT_WHITE,oh?COLOR_START_HOVER:COLOR_ACCENT);
}

static void do_shutdown(void){vesa_fill_screen(COLOR_BLACK);vesa_draw_string(320,290,"Kapatiliyor...",COLOR_TEXT_WHITE,COLOR_BLACK);vesa_copy_buffer();outb(0x604,0x2000);outb(0xB004,0x2000);outb(0x4004,0x3400);__asm__ __volatile__("cli;hlt");}

/* ======== ANA DONGU ======== */
void gui_run(void){
    mouse_state_t ms;
    int sh2=vesa_get_height(),sw2=vesa_get_width();
    vesa_detect_monitor();
    monitor_info_t*mi=vesa_get_monitor_info();
    settings_selected_depth=vesa_get_depth();
    selected_hz=current_hz;
    /* Adaptor ID kaydet */
    mi->adapter_id = inw(0x01CF); /* son okunan deger */
    outw(0x01CE,0); mi->adapter_id = inw(0x01CF);

    while(1){
        mouse_poll(&ms);
        vesa_fill_screen(COLOR_BG);
        draw_icons();
        if(window_open==1)show_sysinfo();
        if(window_open==2)show_about();
        if(window_open==3)show_depth_settings(ms.x,ms.y);
        if(window_open==4)show_hz_settings(ms.x,ms.y);
        if(window_open==5||window_open==6)show_error(ms.x,ms.y);
        draw_taskbar();
        if(start_open)draw_start_menu(ms.x,ms.y);
        mouse_draw_cursor(ms.x,ms.y);
        vesa_copy_buffer();
        if(!ms.click)continue;

        /* Hata popup */
        if(window_open==5||window_open==6){int pw=400,ph=240,px=sw2/2-pw/2,py=sh2/2-ph/2;
        if((ms.x>=px+pw-24&&ms.x<px+pw-8&&ms.y>=py+8&&ms.y<py+24)||(ms.x>=px+pw/2-60&&ms.x<px+pw/2+60&&ms.y>=py+ph-45&&ms.y<py+ph-13))
        {window_open=(window_open==5)?3:4;}continue;}
        /* Kapat butonlari */
        if(window_open==1&&ms.x>=150+500-24&&ms.x<150+500-8&&ms.y>=40+8&&ms.y<40+24){window_open=0;continue;}
        if(window_open==2&&ms.x>=200+400-24&&ms.x<200+400-8&&ms.y>=120+8&&ms.y<120+24){window_open=0;continue;}
        if(window_open==3){int wx=130,wy=30,ww=540,wh=510;
            if(ms.x>=wx+ww-24&&ms.x<wx+ww-8&&ms.y>=wy+8&&ms.y<wy+24){window_open=0;continue;}
            if(ms.x>=wx+180&&ms.x<wx+340&&ms.y>=wy+38&&ms.y<wy+64){window_open=4;selected_hz=current_hz;continue;}
            for(int i=0;i<9;i++){int bx=wx+15+(i%3)*175,by=wy+122+(i/3)*52;if(ms.x>=bx&&ms.x<bx+165&&ms.y>=by&&ms.y<by+46){settings_selected_depth=dv[i];break;}}
            int abx=wx+ww/2-80,aby=wy+wh-42;
            if(ms.x>=abx&&ms.x<abx+160&&ms.y>=aby&&ms.y<aby+32){
                int sup=0;for(int i=0;i<9;i++)if(dv[i]==settings_selected_depth)sup=mi->sup_depth[i];
                if(sup){vesa_set_depth(settings_selected_depth);window_open=0;}
                else{k_strcpy(err_title,"Hata - Monitor Desteklemiyor");k_strcpy(err_l1,"Monitor bu renk derinligini");
                char t[40];k_strcpy(t,"desteklemiyor! Secilen: ");char d[6];k_itoa(settings_selected_depth,d,10);int tl=k_strlen(t);k_strcpy(t+tl,d);tl=k_strlen(t);t[tl]='-';t[tl+1]='b';t[tl+2]='i';t[tl+3]='t';t[tl+4]=0;k_strcpy(err_l2,t);
                k_strcpy(err_l3,"Donanim: ");char m[6];k_itoa(mi->max_bpp,m,10);int tl2=k_strlen(err_l3);k_strcpy(err_l3+tl2,m);tl2=k_strlen(err_l3);k_strcpy(err_l3+tl2,"-bit panel");
                window_open=5;}}continue;}
        if(window_open==4){int wx=130,wy=30,ww=540,wh=510;
            if(ms.x>=wx+ww-24&&ms.x<wx+ww-8&&ms.y>=wy+8&&ms.y<wy+24){window_open=0;continue;}
            if(ms.x>=wx+10&&ms.x<wx+170&&ms.y>=wy+38&&ms.y<wy+64){window_open=3;settings_selected_depth=vesa_get_depth();continue;}
            for(int i=0;i<9;i++){int bx=wx+15+(i%3)*175,by=wy+132+(i/3)*55;if(ms.x>=bx&&ms.x<bx+165&&ms.y>=by&&ms.y<by+48){selected_hz=hv[i];break;}}
            int abx=wx+ww/2-80,aby=wy+wh-42;
            if(ms.x>=abx&&ms.x<abx+160&&ms.y>=aby&&ms.y<aby+32){
                int sup=0;for(int i=0;i<9;i++)if(hv[i]==selected_hz)sup=mi->sup_hz[i];
                if(sup){current_hz=selected_hz;window_open=0;}
                else{k_strcpy(err_title,"Hata - Monitor Desteklemiyor");k_strcpy(err_l1,"Monitor bu yenileme hizini");
                char t[40];k_strcpy(t,"desteklemiyor! Secilen: ");char d[6];k_itoa(selected_hz,d,10);int tl=k_strlen(t);k_strcpy(t+tl,d);tl=k_strlen(t);t[tl]=' ';t[tl+1]='H';t[tl+2]='z';t[tl+3]=0;k_strcpy(err_l2,t);
                k_strcpy(err_l3,"Algilanan max: ");char m[6];k_itoa(mi->max_hz,m,10);int tl2=k_strlen(err_l3);k_strcpy(err_l3+tl2,m);tl2=k_strlen(err_l3);k_strcpy(err_l3+tl2," Hz");
                window_open=6;}}continue;}
        if(ms.x>=4&&ms.x<84&&ms.y>=sh2-36&&ms.y<sh2-4){start_open=!start_open;window_open=0;continue;}
        if(ms.x>=sw2-85&&ms.x<sw2-55&&ms.y>=sh2-32&&ms.y<sh2-8){window_open=3;settings_selected_depth=vesa_get_depth();start_open=0;continue;}
        if(start_open){int smy=sh2-40-220;for(int i=0;i<5;i++){int iy=smy+50+i*32;if(ms.x>=0&&ms.x<240&&ms.y>=iy&&ms.y<iy+32){start_open=0;if(i==0)gui_terminal();else if(i==1)window_open=1;else if(i==2)window_open=2;else if(i==3)outb(0x64,0xFE);else if(i==4)do_shutdown();break;}}if(ms.x>240||ms.y<(sh2-40-220))start_open=0;continue;}
        for(int i=0;i<DICON_COUNT;i++){int ix=dicons[i].x,iy=dicons[i].y;if(ms.x>=ix&&ms.x<ix+60&&ms.y>=iy&&ms.y<iy+50){if(dicons[i].id==1)window_open=1;else if(dicons[i].id==2)gui_terminal();else if(dicons[i].id==3)window_open=2;else{window_open=3;settings_selected_depth=vesa_get_depth();}start_open=0;break;}}
    }
}
