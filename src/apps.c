#include "apps.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"
#include "vfs.h"
#include "heap.h"

/* ============================================================ */
/*                     DOSYA GEZGINI (VFS)                      */
/* ============================================================ */

static int  fe_dir = -1;       /* mevcut klasor (-1=root) */
static int  fe_selected = -1;  /* secili dosya/klasor */
static int  fe_preview = -1;   /* onizleme dosyasi */

void app_file_explorer(void) {
    mouse_state_t ms;
    int wx = 40, wy = 20, ww = 720, wh = 530;
    fe_dir = -1;
    fe_selected = -1;
    fe_preview = -1;

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();

        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;
        if (key == '\b' && fe_dir != -1) {
            vfs_node_t* dn = vfs_get_node(fe_dir);
            if (dn) fe_dir = dn->parent;
            else fe_dir = -1;
            fe_selected = -1;
            fe_preview = -1;
        }

        /* --- Cizim --- */
        vesa_fill_screen(COLOR_BG);

        /* Pencere */
        vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
        vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
        draw_folder_icon(wx + 8, wy + 8);
        vesa_draw_string(wx + 28, wy + 8, "Dosya Gezgini", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

        /* Kapat butonu */
        int close_x = wx + ww - 28, close_y = wy + 4;
        int close_hover = (ms.x >= close_x && ms.x < close_x + 24 &&
                           ms.y >= close_y && ms.y < close_y + 24);
        vesa_fill_rect(close_x, close_y, 24, 24, close_hover ? 0xFF2233 : COLOR_CLOSE_BTN);
        for (int i = 0; i < 8; i++) {
            vesa_putpixel(close_x+4+i, close_y+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(close_x+5+i, close_y+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(close_x+11-i, close_y+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(close_x+12-i, close_y+4+i, COLOR_TEXT_WHITE);
        }

        /* Adres cubugu */
        vesa_fill_rect(wx + 10, wy + 38, ww - 20, 24, 0x1A1A2A);
        vesa_draw_rect_outline(wx + 10, wy + 38, ww - 20, 24, COLOR_WINDOW_BORDER);
        char path[VFS_MAX_PATH];
        vfs_build_path(fe_dir, path);
        vesa_draw_string(wx + 16, wy + 42, path, COLOR_TEXT_CYAN, 0x1A1A2A);

        /* Sol panel */
        int lx = wx + 10, ly = wy + 68;
        int lw = (fe_preview >= 0) ? 380 : (ww - 20);
        int lh = wh - 78;
        vesa_fill_rect(lx, ly, lw, lh, 0x1E1E30);
        vesa_draw_rect_outline(lx, ly, lw, lh, COLOR_WINDOW_BORDER);

        int item_y = ly + 4;

        /* Geri butonu */
        if (fe_dir != -1) {
            int bh2 = (ms.x >= lx+4 && ms.x < lx+lw-4 &&
                       ms.y >= item_y && ms.y < item_y+22);
            if (bh2) vesa_fill_rect(lx+2, item_y, lw-4, 22, COLOR_MENU_HOVER);
            draw_back_icon(lx + 8, item_y + 3);
            vesa_draw_string(lx + 26, item_y + 3, ".. (Ust Klasor)",
                             COLOR_TEXT_GREY, bh2 ? COLOR_MENU_HOVER : 0x1E1E30);
            item_y += 26;
        }

        /* Dosyalar — VFS'ten oku */
        int child_count = vfs_count_children(fe_dir);
        for (int idx = 0; idx < child_count && item_y < ly + lh - 4; idx++) {
            int fi = vfs_get_child(fe_dir, idx);
            vfs_node_t* n = vfs_get_node(fi);
            if (!n) continue;

            int bh2 = (ms.x >= lx+4 && ms.x < lx+lw-4 &&
                       ms.y >= item_y && ms.y < item_y+22);
            int is_sel = (fi == fe_selected);
            uint32_t row_bg = is_sel ? COLOR_ACCENT : (bh2 ? 0x2A2A44 : 0x1E1E30);

            vesa_fill_rect(lx + 2, item_y, lw - 4, 22, row_bg);

            if (n->type == VFS_DIRECTORY)
                draw_folder_icon(lx + 8, item_y + 3);
            else
                draw_file_icon(lx + 8, item_y + 3);

            vesa_draw_string(lx + 26, item_y + 3, n->name, COLOR_TEXT_WHITE, row_bg);

            if (n->type == VFS_FILE) {
                char sz[12];
                k_itoa((int)n->size, sz, 10);
                int sl2 = k_strlen(sz);
                sz[sl2] = ' '; sz[sl2+1] = 'B'; sz[sl2+2] = '\0';
                vesa_draw_string(lx + lw - 80, item_y + 3, sz, COLOR_TEXT_GREY, row_bg);
            } else {
                vesa_draw_string(lx + lw - 80, item_y + 3, "<DIR>", COLOR_TEXT_YELLOW, row_bg);
            }

            item_y += 24;
        }

        /* Sag panel — onizleme */
        if (fe_preview >= 0) {
            vfs_node_t* pn = vfs_get_node(fe_preview);
            if (pn && pn->type == VFS_FILE) {
                int rx = lx + lw + 6, ry = ly, rw = ww - lw - 26, rh = lh;
                vesa_fill_rect(rx, ry, rw, rh, 0x1A1A2A);
                vesa_draw_rect_outline(rx, ry, rw, rh, COLOR_WINDOW_BORDER);

                vesa_draw_string(rx + 8, ry + 4, pn->name, COLOR_TEXT_YELLOW, 0x1A1A2A);

                char sz2[20];
                k_strcpy(sz2, "Boyut: ");
                k_itoa((int)pn->size, sz2 + 7, 10);
                int sl3 = k_strlen(sz2);
                sz2[sl3] = ' '; sz2[sl3+1] = 'B'; sz2[sl3+2] = '\0';
                vesa_draw_string(rx + 8, ry + 22, sz2, COLOR_TEXT_GREY, 0x1A1A2A);
                vesa_fill_rect(rx + 4, ry + 38, rw - 8, 1, COLOR_WINDOW_BORDER);

                /* Icerik */
                if (pn->data && pn->size > 0) {
                    int tx = rx + 8, ty = ry + 44;
                    for (uint32_t ci = 0; ci < pn->size && ci < 800 && ty < ry + rh - 8; ci++) {
                        char ch = (char)pn->data[ci];
                        if (ch == '\n') { tx = rx + 8; ty += 16; }
                        else if (ch >= 32 && ch < 127 && tx < rx + rw - 16) {
                            vesa_draw_char(tx, ty, ch, COLOR_TEXT_WHITE, 0x1A1A2A);
                            tx += 8;
                        }
                    }
                }
            }
        }

        /* Durum cubugu */
        vesa_fill_rect(wx, wy + wh - 20, ww, 20, COLOR_TASKBAR);
        char status[64];
        k_itoa(child_count, status, 10);
        k_strcpy(status + k_strlen(status), " oge");
        if (fe_preview >= 0) {
            k_strcpy(status + k_strlen(status), " | Heap: ");
            k_itoa((int)heap_get_used(), status + k_strlen(status), 10);
            k_strcpy(status + k_strlen(status), " B");
        }
        vesa_draw_string(wx + 8, wy + wh - 16, status, COLOR_TEXT_GREY, COLOR_TASKBAR);
        vesa_draw_string(wx + ww - 200, wy + wh - 16, "ESC=Kapat  Bksp=Geri",
                         COLOR_TEXT_GREY, COLOR_TASKBAR);

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        /* --- Tiklama --- */
        if (!ms.click) continue;

        /* Kapat */
        if (ms.x >= close_x && ms.x < close_x + 24 &&
            ms.y >= close_y && ms.y < close_y + 24)
            return;

        /* Geri */
        if (fe_dir != -1 && ms.x >= lx+4 && ms.x < lx+lw-4 &&
            ms.y >= ly+4 && ms.y < ly+26) {
            vfs_node_t* dn = vfs_get_node(fe_dir);
            if (dn) fe_dir = dn->parent;
            else fe_dir = -1;
            fe_selected = -1;
            fe_preview = -1;
            continue;
        }

        /* Dosya tiklama */
        int cy = ly + 4 + (fe_dir != -1 ? 26 : 0);
        for (int idx = 0; idx < child_count; idx++) {
            int fi = vfs_get_child(fe_dir, idx);
            vfs_node_t* n = vfs_get_node(fi);
            if (!n) continue;
            if (ms.x >= lx+4 && ms.x < lx+lw-4 &&
                ms.y >= cy && ms.y < cy+22) {
                if (n->type == VFS_DIRECTORY) {
                    fe_dir = fi;
                    fe_selected = -1;
                    fe_preview = -1;
                } else {
                    fe_selected = fi;
                    fe_preview = fi;
                }
                break;
            }
            cy += 24;
        }
    }
}

/* ============================================================ */
/*                       NOT DEFTERI (VFS)                      */
/* ============================================================ */

#define NP_COLS 80
#define NP_ROWS 28
static char np_buf[NP_ROWS][NP_COLS + 1];
static int np_cx, np_cy;

void app_notepad(void) {
    mouse_state_t ms;
    k_memset(np_buf, 0, sizeof(np_buf));
    np_cx = 0;
    np_cy = 0;

    k_strcpy(np_buf[0], "MyOS Not Defteri v0.2");
    k_strcpy(np_buf[1], "---");
    k_strcpy(np_buf[2], "Yazmaya baslayin...");
    np_cy = 3;
    np_cx = 0;

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();

        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;

        if (key == '\n') {
            np_cx = 0;
            np_cy++;
            if (np_cy >= NP_ROWS) {
                for (int r = 0; r < NP_ROWS - 1; r++)
                    k_strcpy(np_buf[r], np_buf[r + 1]);
                k_memset(np_buf[NP_ROWS - 1], 0, NP_COLS + 1);
                np_cy = NP_ROWS - 1;
            }
        } else if (key == '\b') {
            if (np_cx > 0) { np_cx--; np_buf[np_cy][np_cx] = ' '; }
            else if (np_cy > 0) { np_cy--; np_cx = k_strlen(np_buf[np_cy]); }
        } else if (key >= 32 && key < 127 && np_cx < NP_COLS - 1) {
            np_buf[np_cy][np_cx++] = (char)key;
        }

        int wx = 60, wy = 30, ww = 680, wh = 520;
        vesa_fill_screen(COLOR_BG);
        vesa_fill_rect(wx, wy, ww, wh, 0x1E1E2E);
        vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
        vesa_draw_string(wx + 16, wy + 8, "Not Defteri", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

        int clx = wx + ww - 28, cly = wy + 4;
        int clh = (ms.x >= clx && ms.x < clx+24 && ms.y >= cly && ms.y < cly+24);
        vesa_fill_rect(clx, cly, 24, 24, clh ? 0xFF2233 : COLOR_CLOSE_BTN);
        for (int i = 0; i < 8; i++) {
            vesa_putpixel(clx+4+i, cly+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(clx+5+i, cly+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(clx+11-i, cly+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(clx+12-i, cly+4+i, COLOR_TEXT_WHITE);
        }

        vesa_fill_rect(wx + 8, wy + 38, ww - 16, wh - 56, 0x0C0C0C);
        vesa_draw_rect_outline(wx + 8, wy + 38, ww - 16, wh - 56, COLOR_WINDOW_BORDER);

        for (int r = 0; r < NP_ROWS; r++)
            for (int c = 0; c < NP_COLS; c++) {
                char ch = np_buf[r][c];
                if (ch && ch != ' ')
                    vesa_draw_char(wx + 14 + c * 8, wy + 42 + r * 16, ch,
                                   COLOR_TEXT_WHITE, 0x0C0C0C);
            }

        vesa_fill_rect(wx + 14 + np_cx * 8, wy + 42 + np_cy * 16, 2, 16, 0x55FF55);

        vesa_fill_rect(wx, wy + wh - 18, ww, 18, COLOR_TASKBAR);
        char pos[32];
        k_strcpy(pos, "Satir:");
        k_itoa(np_cy + 1, pos + 6, 10);
        k_strcpy(pos + k_strlen(pos), " Sutun:");
        k_itoa(np_cx + 1, pos + k_strlen(pos), 10);
        vesa_draw_string(wx + 8, wy + wh - 15, pos, COLOR_TEXT_GREY, COLOR_TASKBAR);

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        if (ms.click && ms.x >= clx && ms.x < clx+24 && ms.y >= cly && ms.y < cly+24)
            return;
    }
}

/* ============================================================ */
/*                     HESAP MAKINESI                           */
/* ============================================================ */

void app_calculator(void) {
    mouse_state_t ms;
    int num1 = 0, num2 = 0, result = 0;
    char display[32] = "0";
    int entering = 0;
    char op = 0;
    int has_result = 0;

    const char* buttons[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "0", "C", "=", "+"
    };

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();

        if (key == KEY_ESC) return;
        if (key == KEY_F4 && keyboard_alt_held()) return;

        if (key >= '0' && key <= '9') {
            int d = key - '0';
            if (has_result) { num1=0; num2=0; op=0; has_result=0; entering=0; }
            if (!entering) { num1 = num1*10+d; k_itoa(num1, display, 10); }
            else { num2 = num2*10+d; k_itoa(num2, display, 10); }
        }
        if (key=='+' || key=='-' || key=='*' || key=='/') {
            op = (char)key; entering = 1; num2 = 0;
        }
        if (key == '\n' || key == '=') {
            if (op=='+') result=num1+num2;
            else if (op=='-') result=num1-num2;
            else if (op=='*') result=num1*num2;
            else if (op=='/' && num2!=0) result=num1/num2;
            else result=num1;
            k_itoa(result, display, 10);
            num1=result; has_result=1; entering=0;
        }

        int wx=260, wy=100, ww=280, wh=380;
        vesa_fill_screen(COLOR_BG);
        vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
        vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
        vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
        vesa_draw_string(wx + 16, wy + 8, "Hesap Makinesi", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

        int clx2 = wx+ww-28, cly2 = wy+4;
        int clh2 = (ms.x>=clx2 && ms.x<clx2+24 && ms.y>=cly2 && ms.y<cly2+24);
        vesa_fill_rect(clx2, cly2, 24, 24, clh2 ? 0xFF2233 : COLOR_CLOSE_BTN);
        for (int i = 0; i < 8; i++) {
            vesa_putpixel(clx2+4+i, cly2+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(clx2+5+i, cly2+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(clx2+11-i, cly2+4+i, COLOR_TEXT_WHITE);
            vesa_putpixel(clx2+12-i, cly2+4+i, COLOR_TEXT_WHITE);
        }

        vesa_fill_rect(wx+15, wy+40, ww-30, 50, 0x0C0C0C);
        vesa_draw_rect_outline(wx+15, wy+40, ww-30, 50, COLOR_WINDOW_BORDER);
        int dlen = k_strlen(display);
        vesa_draw_string(wx+ww-25-dlen*8, wy+58, display, COLOR_TEXT_GREEN, 0x0C0C0C);
        if (op) { char os[2]={op,0}; vesa_draw_string(wx+20, wy+45, os, COLOR_TEXT_YELLOW, 0x0C0C0C); }

        for (int i = 0; i < 16; i++) {
            int bx = wx+15+(i%4)*63, by = wy+100+(i/4)*63;
            int bw = 58, bh = 55;
            int hov = (ms.x>=bx && ms.x<bx+bw && ms.y>=by && ms.y<by+bh);
            int is_op = (i%4==3 || i==14);
            uint32_t bg = is_op ? (hov?COLOR_START_HOVER:COLOR_ACCENT) : (hov?COLOR_BUTTON_HOVER:COLOR_BUTTON);
            vesa_fill_rect(bx, by, bw, bh, bg);
            vesa_draw_rect_outline(bx, by, bw, bh, COLOR_WINDOW_BORDER);
            int tw = k_strlen(buttons[i]) * 8;
            vesa_draw_string(bx+(bw-tw)/2, by+20, buttons[i], COLOR_TEXT_WHITE, bg);
        }

        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;

        if (ms.x >= clx2 && ms.x < clx2+24 && ms.y >= cly2 && ms.y < cly2+24) return;

        for (int i = 0; i < 16; i++) {
            int bx = wx+15+(i%4)*63, by = wy+100+(i/4)*63;
            if (ms.x>=bx && ms.x<bx+58 && ms.y>=by && ms.y<by+55) {
                const char* b = buttons[i];
                if (b[0]>='0' && b[0]<='9') {
                    int d = b[0]-'0';
                    if (has_result) { num1=0; num2=0; op=0; has_result=0; entering=0; }
                    if (!entering) { num1=num1*10+d; k_itoa(num1,display,10); }
                    else { num2=num2*10+d; k_itoa(num2,display,10); }
                } else if (b[0]=='C') {
                    num1=0; num2=0; result=0; op=0; entering=0; has_result=0;
                    k_strcpy(display,"0");
                } else if (b[0]=='=') {
                    if (op=='+') result=num1+num2;
                    else if (op=='-') result=num1-num2;
                    else if (op=='*') result=num1*num2;
                    else if (op=='/' && num2) result=num1/num2;
                    else result=num1;
                    k_itoa(result,display,10); num1=result; has_result=1; entering=0;
                } else {
                    op=b[0]; entering=1; num2=0;
                }
                break;
            }
        }
    }
}
