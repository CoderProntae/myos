#include "apps.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"

/* ============================================================ */
/*                    SANAL DOSYA SISTEMI                        */
/* ============================================================ */

#define MAX_FILES 16
#define MAX_PATH  128

typedef struct {
    char name[32];
    int  is_dir;
    int  parent;       /* ust klasor id, -1=root */
    int  size;         /* dosya boyutu byte */
    const char* content;
} vfile_t;

static vfile_t vfs[] = {
    /* 0  */ {"Belgeler",     1, -1, 0, 0},
    /* 1  */ {"Sistem",       1, -1, 0, 0},
    /* 2  */ {"Programlar",   1, -1, 0, 0},
    /* 3  */ {"readme.txt",   0, 0, 156,
              "MyOS v0.3 - Hosgeldiniz!\n\n"
              "Bu isletim sistemi sifirdan\n"
              "C ve Assembly ile yazildi.\n\n"
              "Komutlar icin terminali acin."},
    /* 4  */ {"notlar.txt",   0, 0, 89,
              "Yapilacaklar:\n"
              "- Dosya sistemi ekle\n"
              "- Ag destegi ekle\n"
              "- Tarayici entegre et"},
    /* 5  */ {"kernel.bin",   0, 1, 32768, "[Binary: Cekirdek dosyasi]"},
    /* 6  */ {"config.sys",   0, 1, 245,
              "[Sistem Yapilandirmasi]\n"
              "boot=grub\n"
              "video=vesa,800x600x32\n"
              "mouse=ps2\n"
              "keyboard=ps2"},
    /* 7  */ {"suruculer",    1, 1, 0, 0},
    /* 8  */ {"terminal.app", 0, 2, 4096,  "[Program: Terminal]"},
    /* 9  */ {"gezgin.app",   0, 2, 3584,  "[Program: Dosya Gezgini]"},
    /* 10 */ {"hesap.app",    0, 2, 2048,  "[Program: Hesap Makinesi]"},
    /* 11 */ {"vga.sys",      0, 7, 8192,  "[Surucu: VGA/VESA]"},
    /* 12 */ {"mouse.sys",    0, 7, 4096,  "[Surucu: PS/2 Mouse]"},
    /* 13 */ {"kb.sys",       0, 7, 3072,  "[Surucu: PS/2 Klavye]"},
    /* 14 */ {"merhaba.txt",  0, 0, 42,    "Merhaba Dunya!\nBu bir test dosyasidir."},
    /* 15 */ {"resimler",     1, -1,0, 0},
};
#define VFS_COUNT 16

static int  current_dir = -1;  /* -1 = root */
static int  selected_file = -1;
static int  preview_file = -1;
static int  scroll_offset = 0;

/* Klasordeki dosya sayisi */
static int count_children(int parent) {
    int c = 0;
    for (int i = 0; i < VFS_COUNT; i++)
        if (vfs[i].parent == parent) c++;
    return c;
}

/* n. cocugu bul */
static int get_child(int parent, int n) {
    int c = 0;
    for (int i = 0; i < VFS_COUNT; i++) {
        if (vfs[i].parent == parent) {
            if (c == n) return i;
            c++;
        }
    }
    return -1;
}

/* Yol stringi olustur */
static void build_path(int dir, char* path) {
    if (dir == -1) { k_strcpy(path, "/"); return; }
    /* Basit: parent/name */
    char tmp[64];
    if (vfs[dir].parent == -1) {
        path[0] = '/'; k_strcpy(path + 1, vfs[dir].name);
    } else {
        build_path(vfs[dir].parent, tmp);
        k_strcpy(path, tmp);
        int l = k_strlen(path);
        path[l] = '/';
        k_strcpy(path + l + 1, vfs[dir].name);
    }
}

/* 16x16 klasor ikonu */
static void draw_folder_icon(int x, int y) {
    vesa_fill_rect(x, y+2, 6, 2, 0xDDAA22);
    vesa_fill_rect(x, y+4, 14, 10, 0xFFCC33);
    vesa_draw_rect_outline(x, y+4, 14, 10, 0xAA8811);
}

/* 16x16 dosya ikonu */
static void draw_file_icon(int x, int y, const char* name) {
    vesa_fill_rect(x+1, y+1, 12, 14, 0xEEEEEE);
    vesa_draw_rect_outline(x+1, y+1, 12, 14, 0x888888);
    /* Dosya tipi rengi */
    uint32_t tc = 0x4488CC;
    int nl = k_strlen(name);
    if (nl > 4 && name[nl-4] == '.') {
        if (name[nl-3]=='t' && name[nl-2]=='x' && name[nl-1]=='t') tc = 0x44AA44;
        if (name[nl-3]=='s' && name[nl-2]=='y' && name[nl-1]=='s') tc = 0xCC4444;
        if (name[nl-3]=='a' && name[nl-2]=='p' && name[nl-1]=='p') tc = 0x4444CC;
        if (name[nl-3]=='b' && name[nl-2]=='i' && name[nl-1]=='n') tc = 0xAA44AA;
    }
    vesa_fill_rect(x+3, y+4, 8, 2, tc);
    vesa_fill_rect(x+3, y+8, 8, 2, tc);
}

static void draw_back_icon(int x, int y) {
    vesa_fill_rect(x+2, y+6, 10, 4, 0x6688CC);
    vesa_fill_rect(x+2, y+4, 4, 8, 0x6688CC);
}

void app_file_explorer(void) {
    mouse_state_t ms;
    int wx=40, wy=20, ww=720, wh=530;
    current_dir = -1;
    selected_file = -1;
    preview_file = -1;
    scroll_offset = 0;

    while (1) {
        mouse_poll(&ms);
        char key = keyboard_poll();

        /* Alt+F4 veya ESC = cik */
        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;
        /* Backspace = ust klasor */
        if (key == '\b' && current_dir != -1) {
            current_dir = vfs[current_dir].parent;
            selected_file = -1; preview_file = -1; scroll_offset = 0;
        }

        /* --- Cizim --- */
        vesa_fill_screen(COLOR_BG);

        /* Pencere */
        vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
        vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
        draw_folder_icon(wx+8, wy+8);
        vesa_draw_string(wx+28, wy+8, "Dosya Gezgini", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
        draw_close_button(wx+ww-24, wy+8, COLOR_CLOSE_BTN);

        /* Adres cubugu */
        vesa_fill_rect(wx+10, wy+38, ww-20, 24, 0x1A1A2A);
        vesa_draw_rect_outline(wx+10, wy+38, ww-20, 24, COLOR_WINDOW_BORDER);
        char path[MAX_PATH];
        build_path(current_dir, path);
        vesa_draw_string(wx+16, wy+42, path, COLOR_TEXT_CYAN, 0x1A1A2A);

        /* Sol panel - dosya listesi */
        int lx = wx+10, ly = wy+68;
        int lw = preview_file >= 0 ? 380 : ww-20;
        int lh = wh - 78;
        vesa_fill_rect(lx, ly, lw, lh, 0x1E1E30);
        vesa_draw_rect_outline(lx, ly, lw, lh, COLOR_WINDOW_BORDER);

        /* Geri butonu */
        int item_y = ly + 4;
        if (current_dir != -1) {
            int bh2 = (ms.x >= lx+4 && ms.x < lx+lw-4 && ms.y >= item_y && ms.y < item_y+22);
            if (bh2) vesa_fill_rect(lx+2, item_y, lw-4, 22, COLOR_MENU_HOVER);
            draw_back_icon(lx+8, item_y+3);
            vesa_draw_string(lx+26, item_y+3, ".. (Ust Klasor)", COLOR_TEXT_GREY, bh2?COLOR_MENU_HOVER:0x1E1E30);
            item_y += 26;
        }

        /* Dosyalar */
        int child_count = count_children(current_dir);
        for (int idx = 0; idx < child_count && item_y < ly+lh-4; idx++) {
            int fi = get_child(current_dir, idx);
            if (fi < 0) continue;

            int bh2 = (ms.x >= lx+4 && ms.x < lx+lw-4 && ms.y >= item_y && ms.y < item_y+22);
            int is_sel = (fi == selected_file);

            uint32_t row_bg = is_sel ? COLOR_ACCENT : bh2 ? 0x2A2A44 : 0x1E1E30;
            vesa_fill_rect(lx+2, item_y, lw-4, 22, row_bg);

            if (vfs[fi].is_dir)
                draw_folder_icon(lx+8, item_y+3);
            else
                draw_file_icon(lx+8, item_y+3);

            vesa_draw_string(lx+26, item_y+3, vfs[fi].name, COLOR_TEXT_WHITE, row_bg);

            if (!vfs[fi].is_dir) {
                char sz[12];
                k_itoa(vfs[fi].size, sz, 10);
                int sl2 = k_strlen(sz);
                sz[sl2]=' '; sz[sl2+1]='B'; sz[sl2+2]='\0';
                vesa_draw_string(lx+lw-80, item_y+3, sz, COLOR_TEXT_GREY, row_bg);
            } else {
                vesa_draw_string(lx+lw-80, item_y+3, "<DIR>", COLOR_TEXT_YELLOW, row_bg);
            }

            item_y += 24;
        }

        /* Sag panel - onizleme */
        if (preview_file >= 0 && !vfs[preview_file].is_dir) {
            int rx = lx+lw+6, ry = ly, rw = ww-lw-26, rh = lh;
            vesa_fill_rect(rx, ry, rw, rh, 0x1A1A2A);
            vesa_draw_rect_outline(rx, ry, rw, rh, COLOR_WINDOW_BORDER);

            vesa_draw_string(rx+8, ry+4, vfs[preview_file].name, COLOR_TEXT_YELLOW, 0x1A1A2A);
            /* Boyut */
            char sz2[20]; k_strcpy(sz2, "Boyut: ");
            k_itoa(vfs[preview_file].size, sz2+7, 10);
            int sl3 = k_strlen(sz2);
            sz2[sl3]=' '; sz2[sl3+1]='B'; sz2[sl3+2]='\0';
            vesa_draw_string(rx+8, ry+22, sz2, COLOR_TEXT_GREY, 0x1A1A2A);
            vesa_fill_rect(rx+4, ry+38, rw-8, 1, COLOR_WINDOW_BORDER);

            /* Icerik */
            if (vfs[preview_file].content) {
                const char* txt = vfs[preview_file].content;
                int tx = rx+8, ty = ry+44;
                while (*txt && ty < ry+rh-8) {
                    if (*txt == '\n') { tx = rx+8; ty += 16; }
                    else if (*txt == '[') {
                        vesa_draw_char(tx, ty, *txt, COLOR_TEXT_CYAN, 0x1A1A2A);
                        tx += 8;
                    } else {
                        vesa_draw_char(tx, ty, *txt, COLOR_TEXT_WHITE, 0x1A1A2A);
                        tx += 8;
                    }
                    if (tx > rx+rw-16) { tx = rx+8; ty += 16; }
                    txt++;
                }
            }
        }

        /* Durum cubugu */
        vesa_fill_rect(wx, wy+wh-20, ww, 20, COLOR_TASKBAR);
        char status[64];
        k_itoa(child_count, status, 10);
        int stl = k_strlen(status);
        k_strcpy(status+stl, " oge");
        vesa_draw_string(wx+8, wy+wh-16, status, COLOR_TEXT_GREY, COLOR_TASKBAR);
        vesa_draw_string(wx+ww-200, wy+wh-16, "ESC=Kapat  Backspace=Geri", COLOR_TEXT_GREY, COLOR_TASKBAR);

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        /* --- Tiklama --- */
        if (!ms.click) continue;

        /* Kapat */
        if (ms.x>=wx+ww-24 && ms.x<wx+ww-8 && ms.y>=wy+8 && ms.y<wy+24)
            return;

        /* Geri butonu */
        if (current_dir != -1 && ms.x>=lx+4 && ms.x<lx+lw-4 && ms.y>=ly+4 && ms.y<ly+26) {
            current_dir = vfs[current_dir].parent;
            selected_file=-1; preview_file=-1;
            continue;
        }

        /* Dosya tiklama */
        int cy = ly + 4 + (current_dir != -1 ? 26 : 0);
        for (int idx=0; idx<child_count; idx++) {
            int fi = get_child(current_dir, idx);
            if (fi<0) continue;
            if (ms.x>=lx+4 && ms.x<lx+lw-4 && ms.y>=cy && ms.y<cy+22) {
                if (vfs[fi].is_dir) {
                    current_dir = fi;
                    selected_file=-1; preview_file=-1;
                } else {
                    selected_file = fi;
                    preview_file = fi;
                }
                break;
            }
            cy += 24;
        }
    }
}

/* ============================================================ */
/*                       NOT DEFTERI                             */
/* ============================================================ */

#define NP_COLS 80
#define NP_ROWS 28
static char np_buf[NP_ROWS][NP_COLS+1];
static int np_cx, np_cy;

void app_notepad(void) {
    mouse_state_t ms;
    k_memset(np_buf, 0, sizeof(np_buf));
    np_cx = 0; np_cy = 0;

    /* Baslangic metni */
    k_strcpy(np_buf[0], "MyOS Not Defteri v0.1");
    k_strcpy(np_buf[1], "---");
    k_strcpy(np_buf[2], "Yazmaya baslayin...");
    np_cy = 3; np_cx = 0;

    while (1) {
        mouse_poll(&ms);
        char key = keyboard_poll();

        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;

        if (key == '\n') {
            np_cx=0; np_cy++;
            if (np_cy>=NP_ROWS) {
                for(int r=0;r<NP_ROWS-1;r++) k_strcpy(np_buf[r],np_buf[r+1]);
                k_memset(np_buf[NP_ROWS-1],0,NP_COLS+1); np_cy=NP_ROWS-1;
            }
        } else if (key == '\b') {
            if (np_cx>0) { np_cx--; np_buf[np_cy][np_cx]=' '; }
            else if (np_cy>0) { np_cy--; np_cx=k_strlen(np_buf[np_cy]); }
        } else if (key >= 32 && key < 127 && np_cx < NP_COLS-1) {
            np_buf[np_cy][np_cx++] = key;
        }

        /* Cizim */
        int wx=60, wy=30, ww=680, wh=520;
        vesa_fill_screen(COLOR_BG);
        vesa_fill_rect(wx,wy,ww,wh,0x1E1E2E);
        vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
        vesa_draw_string(wx+16,wy+8,"Not Defteri",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
        draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

        /* Metin alani */
        vesa_fill_rect(wx+8,wy+38,ww-16,wh-56,0x0C0C0C);
        vesa_draw_rect_outline(wx+8,wy+38,ww-16,wh-56,COLOR_WINDOW_BORDER);

        for (int r=0; r<NP_ROWS; r++)
            for (int c=0; c<NP_COLS; c++) {
                char ch = np_buf[r][c];
                if (ch && ch != ' ')
                    vesa_draw_char(wx+14+c*8, wy+42+r*16, ch, COLOR_TEXT_WHITE, 0x0C0C0C);
            }

        /* Cursor */
        vesa_fill_rect(wx+14+np_cx*8, wy+42+np_cy*16, 8, 16, 0x888888);

        /* Durum */
        vesa_fill_rect(wx,wy+wh-18,ww,18,COLOR_TASKBAR);
        char pos[20]; k_strcpy(pos,"Satir:");
        k_itoa(np_cy+1,pos+6,10); int pl=k_strlen(pos);
        pos[pl]=' '; pos[pl+1]='S'; pos[pl+2]='u'; pos[pl+3]='t'; pos[pl+4]='u'; pos[pl+5]='n'; pos[pl+6]=':'; pos[pl+7]='\0';
        k_itoa(np_cx+1,pos+k_strlen(pos),10);
        vesa_draw_string(wx+8,wy+wh-15,pos,COLOR_TEXT_GREY,COLOR_TASKBAR);

        mouse_draw_cursor(ms.x,ms.y);
        vesa_copy_buffer();

        if (ms.click && ms.x>=wx+ww-24 && ms.x<wx+ww-8 && ms.y>=wy+8 && ms.y<wy+24)
            return;
    }
}

/* ============================================================ */
/*                     HESAP MAKINESI                           */
/* ============================================================ */

void app_calculator(void) {
    mouse_state_t ms;
    int num1=0, num2=0, result=0;
    char display[32] = "0";
    int entering=0; /* 0=num1, 1=num2 */
    char op = 0;
    int has_result = 0;

    const char* buttons[] = {
        "7","8","9","/",
        "4","5","6","*",
        "1","2","3","-",
        "0","C","=","+"
    };

    while (1) {
        mouse_poll(&ms);
        char key = keyboard_poll();
        if (key==KEY_ESC) return;
        if (key==KEY_F4 && keyboard_alt_held()) return;

        /* Klavye girisi */
        if (key>='0' && key<='9') {
            int d = key-'0';
            if (has_result) { num1=0; num2=0; op=0; has_result=0; entering=0; }
            if (!entering) { num1=num1*10+d; k_itoa(num1,display,10); }
            else { num2=num2*10+d; k_itoa(num2,display,10); }
        }
        if (key=='+'||key=='-'||key=='*'||key=='/') {
            op=key; entering=1; num2=0;
        }
        if (key=='\n' || key=='=') {
            if (op=='+') result=num1+num2;
            else if (op=='-') result=num1-num2;
            else if (op=='*') result=num1*num2;
            else if (op=='/' && num2!=0) result=num1/num2;
            else result=num1;
            k_itoa(result,display,10);
            num1=result; has_result=1; entering=0;
        }

        /* Cizim */
        int wx=260,wy=100,ww=280,wh=380;
        vesa_fill_screen(COLOR_BG);
        vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG);
        vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
        vesa_draw_string(wx+16,wy+8,"Hesap Makinesi",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
        draw_close_button(wx+ww-24,wy+8,COLOR_CLOSE_BTN);

        /* Ekran */
        vesa_fill_rect(wx+15,wy+40,ww-30,50,0x0C0C0C);
        vesa_draw_rect_outline(wx+15,wy+40,ww-30,50,COLOR_WINDOW_BORDER);
        int dlen=k_strlen(display);
        vesa_draw_string(wx+ww-25-dlen*8,wy+58,display,COLOR_TEXT_GREEN,0x0C0C0C);
        if (op) { char os[2]={op,0}; vesa_draw_string(wx+20,wy+45,os,COLOR_TEXT_YELLOW,0x0C0C0C); }

        /* Butonlar 4x4 */
        for (int i=0;i<16;i++) {
            int bx=wx+15+(i%4)*63, by=wy+100+(i/4)*63;
            int bw=58, bh=55;
            int hov=(ms.x>=bx&&ms.x<bx+bw&&ms.y>=by&&ms.y<by+bh);
            int is_op=(i%4==3 || i==14); /* operator veya = */
            uint32_t bg = is_op ? (hov?COLOR_START_HOVER:COLOR_ACCENT) : (hov?COLOR_BUTTON_HOVER:COLOR_BUTTON);
            vesa_fill_rect(bx,by,bw,bh,bg);
            vesa_draw_rect_outline(bx,by,bw,bh,COLOR_WINDOW_BORDER);
            int tw=k_strlen(buttons[i])*8;
            vesa_draw_string(bx+(bw-tw)/2,by+20,buttons[i],COLOR_TEXT_WHITE,bg);
        }

        mouse_draw_cursor(ms.x,ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;
        /* Kapat */
        if (ms.x>=wx+ww-24&&ms.x<wx+ww-8&&ms.y>=wy+8&&ms.y<wy+24) return;
        /* Buton tiklama */
        for (int i=0;i<16;i++) {
            int bx=wx+15+(i%4)*63, by=wy+100+(i/4)*63;
            if (ms.x>=bx&&ms.x<bx+58&&ms.y>=by&&ms.y<by+55) {
                const char* b=buttons[i];
                if (b[0]>='0'&&b[0]<='9') {
                    int d=b[0]-'0';
                    if (has_result){num1=0;num2=0;op=0;has_result=0;entering=0;}
                    if(!entering){num1=num1*10+d;k_itoa(num1,display,10);}
                    else{num2=num2*10+d;k_itoa(num2,display,10);}
                } else if (b[0]=='C') {
                    num1=0;num2=0;result=0;op=0;entering=0;has_result=0;
                    k_strcpy(display,"0");
                } else if (b[0]=='=') {
                    if(op=='+')result=num1+num2;else if(op=='-')result=num1-num2;
                    else if(op=='*')result=num1*num2;else if(op=='/'&&num2)result=num1/num2;
                    else result=num1;
                    k_itoa(result,display,10);num1=result;has_result=1;entering=0;
                } else {
                    op=b[0];entering=1;num2=0;
                }
                break;
            }
        }
    }
}
