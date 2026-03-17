#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"
#include "apps.h"
#include "browser.h"
#include "pci.h"
#include "e1000.h"
#include "net.h"
#include "dns.h"
#include "tcp.h"
#include "heap.h"
#include "vfs.h"
#include "elf.h"
#include "syscall.h"
#include "task.h"
#include "libc.h"
#include "posix.h"

static int start_open = 0, window_open = 0;
static int settings_selected_depth = 32, current_hz = 60, selected_hz = 60;
static const int dv[9] = {1,4,8,16,24,30,32,36,48};
static const int hv[9] = {30,50,60,75,100,120,144,165,240};

static uint8_t bcd2bin(uint8_t b) { return ((b>>4)*10)+(b&0x0F); }

static void get_time(char* buf) {
    outb(0x70,4); uint8_t h = bcd2bin(inb(0x71));
    outb(0x70,2); uint8_t m = bcd2bin(inb(0x71));
    buf[0]='0'+h/10; buf[1]='0'+h%10; buf[2]=':';
    buf[3]='0'+m/10; buf[4]='0'+m%10; buf[5]=0;
}

static const char* depth_info(int d) {
    switch(d) {
        case 1:return"2 renk";case 4:return"64 renk";case 8:return"256 renk";
        case 16:return"65K renk";case 24:return"16.7M renk";case 30:return"1.07 Milyar";
        case 32:return"16.7M+Alpha";case 36:return"68.7 Milyar";case 48:return"281 Trilyon";
        default:return"";
    }
}

static const char* hz_info(int h) {
    switch(h) {
        case 30:return"Sinema";case 50:return"PAL";case 60:return"Standart";
        case 75:return"Gelismis";case 100:return"Yuksek";case 120:return"Oyun";
        case 144:return"Gaming";case 165:return"Gaming+";case 240:return"E-spor";
        default:return"";
    }
}

/* ======== TERMINAL (RENKLI) ======== */
#define GT_COLS 88
#define GT_ROWS 28
static char gt_buf[GT_ROWS][GT_COLS+1];
static uint8_t gt_clr[GT_ROWS][GT_COLS];
static int gt_cx, gt_cy;
static char gt_cmd[256];
static int gt_cmd_len;
static uint8_t gt_cur_color;

static const uint32_t gt_palette[] = {
    0xAAAAAA, 0x55FF55, 0x55FFFF, 0xFFFF55,
    0xFF5555, 0xFFFFFF, 0x5599FF,
};

static void gt_clear(void) {
    for (int r = 0; r < GT_ROWS; r++) {
        k_memset(gt_buf[r], 0, GT_COLS + 1);
        k_memset(gt_clr[r], 0, GT_COLS);
    }
    gt_cx = gt_cy = 0;
    gt_cur_color = 0;
}

static void gt_scroll(void) {
    while (gt_cy >= GT_ROWS) {
        for (int r = 0; r < GT_ROWS - 1; r++) {
            for (int c = 0; c < GT_COLS; c++) {
                gt_buf[r][c] = gt_buf[r+1][c];
                gt_clr[r][c] = gt_clr[r+1][c];
            }
            gt_buf[r][GT_COLS] = '\0';
        }
        k_memset(gt_buf[GT_ROWS-1], 0, GT_COLS+1);
        k_memset(gt_clr[GT_ROWS-1], 0, GT_COLS);
        gt_cy--;
    }
}

static void gt_setcolor(uint8_t c) { gt_cur_color = c; }

static void gt_putc(char c) {
    if (c == '\n') { gt_cx = 0; gt_cy++; gt_scroll(); }
    else if (c == '\b') { if (gt_cx > 0) { gt_cx--; gt_buf[gt_cy][gt_cx] = ' '; } }
    else if (gt_cx < GT_COLS) {
        gt_buf[gt_cy][gt_cx] = c;
        gt_clr[gt_cy][gt_cx] = gt_cur_color;
        gt_cx++;
        if (gt_cx >= GT_COLS) { gt_cx = 0; gt_cy++; gt_scroll(); }
    }
}

static void gt_puts(const char* s) { while (*s) gt_putc(*s++); }
static void gt_puts_c(const char* s, uint8_t c) { gt_setcolor(c); gt_puts(s); }

static void gt_render(void) {
    int sw2 = vesa_get_width(), sh2 = vesa_get_height();
    int wx = 20, wy = 10, ww = sw2 - 40, wh = sh2 - 50;
    vesa_fill_screen(COLOR_BG);
    vesa_fill_rect(wx, wy, ww, wh, COLOR_TERMINAL_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx + 16, wy + 8, "Terminal - MyOS", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    for (int r = 0; r < GT_ROWS; r++)
        for (int c = 0; c < GT_COLS; c++) {
            char ch = gt_buf[r][c];
            if (ch && ch != ' ')
                vesa_draw_char(28 + c*8, 50 + r*16, ch,
                    gt_palette[gt_clr[r][c]], COLOR_TERMINAL_BG);
        }
    vesa_fill_rect(28 + gt_cx*8, 50 + gt_cy*16, 2, 16, 0x55FF55);
    vesa_copy_buffer();
}

static void gt_prompt(void) {
    gt_puts_c("myos", 1); gt_puts_c(":", 5);
    gt_puts_c("~", 6); gt_puts_c("$ ", 5);
    gt_setcolor(0);
    gt_cmd_len = 0;
    k_memset(gt_cmd, 0, 256);
}

static int gt_process(void) {
    gt_putc('\n');
    const char* c = gt_cmd;
    while (*c == ' ') c++;

    if (!k_strlen(c)) return 0;
    if (!k_strcmp(c, "exit")) return 1;
    if (!k_strcmp(c, "clear")) { gt_clear(); return 0; }

    if (!k_strcmp(c, "help")) {
        gt_puts_c("  Komutlar:\n", 3);
        gt_puts_c("  help", 2);      gt_puts_c("      - Bu mesaj\n", 0);
        gt_puts_c("  clear", 2);     gt_puts_c("     - Ekrani temizle\n", 0);
        gt_puts_c("  echo", 2);      gt_puts_c("      - Yazi yazdir\n", 0);
        gt_puts_c("  time", 2);      gt_puts_c("      - Saat\n", 0);
        gt_puts_c("  sysinfo", 2);   gt_puts_c("   - Sistem bilgisi\n", 0);
        gt_puts_c("  netinfo", 2);   gt_puts_c("   - Ag bilgisi\n", 0);
        gt_puts_c("  meminfo", 2);   gt_puts_c("   - Bellek bilgisi\n", 0);
        gt_puts_c("  memtest", 2);   gt_puts_c("   - Bellek testi\n", 0);
        gt_puts_c("  ping", 2);      gt_puts_c("      - Gateway'e ping\n", 0);
        gt_puts_c("  nslookup", 2);  gt_puts_c("  - DNS cozumle\n", 0);
        gt_puts_c("  tcptest", 2);   gt_puts_c("   - TCP baglanti testi\n", 0);
        gt_puts_c("  ls", 2);       gt_puts_c("        - Dosya listele\n", 0);
        gt_puts_c("  cat", 2);      gt_puts_c("       - Dosya oku\n", 0);
        gt_puts_c("  touch", 2);    gt_puts_c("     - Bos dosya olustur\n", 0);
        gt_puts_c("  mkdir", 2);    gt_puts_c("     - Klasor olustur\n", 0);
        gt_puts_c("  write", 2);    gt_puts_c("     - Dosyaya yaz\n", 0);
        gt_puts_c("  rm", 2);       gt_puts_c("        - Dosya/klasor sil\n", 0);
        gt_puts_c("  run", 2);      gt_puts_c("       - Program calistir\n", 0);
        gt_puts_c("  elfinfo", 2);  gt_puts_c("   - ELF bilgisi\n", 0);
        gt_puts_c("  syscall", 2);  gt_puts_c("   - Syscall testi\n", 0);
        gt_puts_c("  tasks", 2);    gt_puts_c("     - Gorev listesi\n", 0);
        gt_puts_c("  libctest", 2);  gt_puts_c("  - libc testi\n", 0);
        gt_puts_c("  posixtest", 2); gt_puts_c(" - POSIX testi\n", 0);
        gt_puts_c("  env", 2);      gt_puts_c("       - Ortam degiskenleri\n", 0);
        gt_puts_c("  exit", 2);     gt_puts_c("      - Masaustune don\n", 0);
        gt_puts_c("  reboot", 2);   gt_puts_c("    - Yeniden baslat\n", 0);
        return 0;
    }

    if (!k_strcmp(c, "time")) {
        outb(0x70, 4); uint8_t h = bcd2bin(inb(0x71));
        outb(0x70, 2); uint8_t m = bcd2bin(inb(0x71));
        outb(0x70, 0); uint8_t s = bcd2bin(inb(0x71));
        gt_puts_c("  ", 0);
        char t[9];
        t[0]='0'+h/10; t[1]='0'+h%10; t[2]=':';
        t[3]='0'+m/10; t[4]='0'+m%10; t[5]=':';
        t[6]='0'+s/10; t[7]='0'+s%10; t[8]=0;
        gt_puts_c(t, 3); gt_putc('\n');
        return 0;
    }

    if (!k_strcmp(c, "sysinfo")) {
        char v[13]; uint32_t eb2, ec2, ed2;
        __asm__ __volatile__("cpuid":"=b"(eb2),"=c"(ec2),"=d"(ed2):"a"(0));
        *((uint32_t*)&v[0])=eb2; *((uint32_t*)&v[4])=ed2;
        *((uint32_t*)&v[8])=ec2; v[12]=0;
        monitor_info_t* mi2 = vesa_get_monitor_info();
        gt_puts_c("  CPU     : ", 0); gt_puts_c(v, 1); gt_putc('\n');
        gt_puts_c("  Adaptor : ", 0); gt_puts_c(mi2->adapter_name, 2); gt_putc('\n');
        gt_puts_c("  Ortam   : ", 0);
        gt_puts_c(mi2->is_virtual ? "Sanal Makine" : "Fiziksel", mi2->is_virtual ? 3 : 1);
        gt_putc('\n');
        return 0;
    }

    if (!k_strcmp(c, "netinfo")) {
        e1000_info_t* ni2 = e1000_get_info();
        net_config_t* nc2 = net_get_config();
        if (!ni2->initialized) { gt_puts_c("  Ag karti baslatilmadi.\n", 4); return 0; }
        char ms3[20];
        for (int m=0;m<6;m++){
            uint8_t hi2=ni2->mac[m]>>4, lo2=ni2->mac[m]&0x0F;
            ms3[m*3]=hi2<10?'0'+hi2:'A'+(hi2-10);
            ms3[m*3+1]=lo2<10?'0'+lo2:'A'+(lo2-10);
            ms3[m*3+2]=(m<5)?':':'\0';
        }
        ms3[17]='\0';
        gt_puts_c("  MAC     : ", 0); gt_puts_c(ms3, 1); gt_putc('\n');
        gt_puts_c("  Link    : ", 0);
        if (ni2->link_up) {
            char spd2[16]; k_itoa(ni2->speed, spd2, 10);
            k_strcpy(spd2+k_strlen(spd2), " Mbps");
            gt_puts_c(spd2, 1);
        } else { gt_puts_c("Bagli Degil", 4); }
        gt_putc('\n');
        if (nc2->configured) {
            char ips2[20];
            k_itoa(nc2->our_ip[0],ips2,10); k_strcpy(ips2+k_strlen(ips2),".");
            k_itoa(nc2->our_ip[1],ips2+k_strlen(ips2),10); k_strcpy(ips2+k_strlen(ips2),".");
            k_itoa(nc2->our_ip[2],ips2+k_strlen(ips2),10); k_strcpy(ips2+k_strlen(ips2),".");
            k_itoa(nc2->our_ip[3],ips2+k_strlen(ips2),10);
            gt_puts_c("  IP      : ", 0); gt_puts_c(ips2, 2); gt_putc('\n');
            char gw2[20];
            k_itoa(nc2->gateway_ip[0],gw2,10); k_strcpy(gw2+k_strlen(gw2),".");
            k_itoa(nc2->gateway_ip[1],gw2+k_strlen(gw2),10); k_strcpy(gw2+k_strlen(gw2),".");
            k_itoa(nc2->gateway_ip[2],gw2+k_strlen(gw2),10); k_strcpy(gw2+k_strlen(gw2),".");
            k_itoa(nc2->gateway_ip[3],gw2+k_strlen(gw2),10);
            gt_puts_c("  Gateway : ", 0); gt_puts_c(gw2, 2); gt_putc('\n');
            char dns2[20];
            k_itoa(nc2->dns_ip[0],dns2,10); k_strcpy(dns2+k_strlen(dns2),".");
            k_itoa(nc2->dns_ip[1],dns2+k_strlen(dns2),10); k_strcpy(dns2+k_strlen(dns2),".");
            k_itoa(nc2->dns_ip[2],dns2+k_strlen(dns2),10); k_strcpy(dns2+k_strlen(dns2),".");
            k_itoa(nc2->dns_ip[3],dns2+k_strlen(dns2),10);
            gt_puts_c("  DNS     : ", 0); gt_puts_c(dns2, 2); gt_putc('\n');
        }
        char txs2[10], rxs2[10];
        k_itoa((int)ni2->tx_count, txs2, 10);
        k_itoa((int)ni2->rx_count, rxs2, 10);
        gt_puts_c("  Paket   : TX=", 0); gt_puts_c(txs2, 5);
        gt_puts_c(" RX=", 0); gt_puts_c(rxs2, 5); gt_putc('\n');
        return 0;
    }

    if (!k_strcmp(c, "meminfo")) {
        gt_puts_c("  Bellek Bilgisi:\n", 3);
        char buf[20];
        gt_puts_c("  Fiziksel RAM : ", 0);
        k_itoa((int)ram_get_total_mb(), buf, 10);
        gt_puts_c(buf, 1); gt_puts_c(" MB (", 0);
        k_itoa((int)ram_get_total_kb(), buf, 10);
        gt_puts_c(buf, 0); gt_puts_c(" KB)\n", 0);
        gt_puts_c("  ---\n", 0);
        gt_puts_c("  Heap Toplam  : ", 0);
        k_itoa((int)(heap_get_total()/1024), buf, 10);
        gt_puts_c(buf, 5); gt_puts_c(" KB\n", 0);
        uint32_t used = heap_get_used();
        gt_puts_c("  Heap Kullan. : ", 0);
        if (used >= 1024) { k_itoa((int)(used/1024), buf, 10); gt_puts_c(buf, 3); gt_puts_c(" KB", 0); }
        else { k_itoa((int)used, buf, 10); gt_puts_c(buf, 3); gt_puts_c(" byte", 0); }
        gt_putc('\n');
        uint32_t fr = heap_get_free();
        gt_puts_c("  Heap Bos     : ", 0);
        if (fr >= 1024) { k_itoa((int)(fr/1024), buf, 10); gt_puts_c(buf, 1); gt_puts_c(" KB", 0); }
        else { k_itoa((int)fr, buf, 10); gt_puts_c(buf, 1); gt_puts_c(" byte", 0); }
        gt_putc('\n');
        gt_puts_c("  Bloklar      : ", 0);
        k_itoa((int)heap_get_block_count(), buf, 10);
        gt_puts_c(buf, 2); gt_putc('\n');
        return 0;
    }

    if (!k_strcmp(c, "memtest")) {
        gt_puts_c("  Bellek testi basliyor...\n", 3);
        char b2[20];
        uint32_t start_used = heap_get_used();
        gt_puts_c("  Baslangic: ", 0); k_itoa((int)start_used, b2, 10);
        gt_puts_c(b2, 0); gt_puts_c(" byte\n", 0);
        void* p1 = kmalloc(1024); void* p2 = kmalloc(2048); void* p3 = kmalloc(4096);
        if (p1 && p2 && p3) { gt_puts_c("  [OK] 3 blok ayrildi\n", 1); }
        else { gt_puts_c("  [FAIL] Ayirma basarisiz!\n", 4); if(p1)kfree(p1); if(p2)kfree(p2); if(p3)kfree(p3); return 0; }
        gt_puts_c("  Ayirma sonrasi: ", 0); k_itoa((int)heap_get_used(), b2, 10);
        gt_puts_c(b2, 3); gt_puts_c(" byte\n", 0);
        uint8_t* test = (uint8_t*)p1;
        for (int i = 0; i < 1024; i++) test[i] = (uint8_t)(i & 0xFF);
        int ok = 1;
        for (int i = 0; i < 1024; i++) if (test[i] != (uint8_t)(i & 0xFF)) { ok = 0; break; }
        gt_puts_c(ok ? "  [OK] Yazma/okuma basarili\n" : "  [FAIL] Veri bozulmasi!\n", ok ? 1 : 4);
        kfree(p1); gt_puts_c("  [OK] p1 serbest\n", 1);
        kfree(p2); gt_puts_c("  [OK] p2 serbest\n", 1);
        kfree(p3); gt_puts_c("  [OK] p3 serbest\n", 1);
        uint32_t end_used = heap_get_used();
        gt_puts_c("  Bitis: ", 0); k_itoa((int)end_used, b2, 10);
        gt_puts_c(b2, 0); gt_puts_c(" byte\n", 0);
        if (end_used == start_used) { gt_puts_c("  BELLEK TESTI BASARILI!\n", 1); }
        else { gt_puts_c("  UYARI: Sizinti!\n", 4); }
        return 0;
    }

    if (!k_strcmp(c, "ping")) {
        net_config_t* nc3 = net_get_config();
        if (!nc3->configured) { gt_puts_c("  Ag yapilandirilmamis.\n", 4); return 0; }
        gt_puts_c("  Ping gonderiliyor...\n", 0);
        gt_render();
        int ret = net_send_ping(nc3->gateway_ip, 1);
        if (ret < 0) { gt_puts_c("  Gonderilemedi!\n", 4); return 0; }
        int got = 0;
        for (int w = 0; w < 30; w++) { for (volatile int d = 0; d < 200000; d++); net_poll(); if (net_get_ping_reply()) { got = 1; break; } }
        gt_puts_c(got ? "  Cevap alindi! Ping basarili.\n" : "  Timeout.\n", got ? 1 : 4);
        return 0;
    }

    if (!k_strncmp(c, "nslookup ", 9)) {
        const char* host = c + 9; while (*host == ' ') host++;
        if (!k_strlen(host)) { gt_puts_c("  Kullanim: nslookup alan.adi\n", 3); return 0; }
        gt_puts_c("  ", 0); gt_puts_c(host, 2); gt_puts_c(" cozumleniyor...\n", 0);
        gt_render();
        uint8_t ip_r[4];
        int ret = dns_resolve(host, ip_r);
        if (ret == 0) {
            char ips3[20]; k_itoa(ip_r[0],ips3,10); k_strcpy(ips3+k_strlen(ips3),".");
            k_itoa(ip_r[1],ips3+k_strlen(ips3),10); k_strcpy(ips3+k_strlen(ips3),".");
            k_itoa(ip_r[2],ips3+k_strlen(ips3),10); k_strcpy(ips3+k_strlen(ips3),".");
            k_itoa(ip_r[3],ips3+k_strlen(ips3),10);
            gt_puts_c("  Sonuc: ", 0); gt_puts_c(ips3, 1); gt_putc('\n');
        } else { gt_puts_c("  DNS basarisiz! Hata: ", 4); char e[8]; k_itoa(ret,e,10); gt_puts_c(e,4); gt_putc('\n'); }
        return 0;
    }
    if (!k_strcmp(c, "nslookup")) { gt_puts_c("  Kullanim: nslookup alan.adi\n", 3); return 0; }

    if (!k_strcmp(c, "tcptest")) {
        net_config_t* nc4 = net_get_config();
        if (!nc4->configured) { gt_puts_c("  Ag yapilandirilmamis.\n", 4); return 0; }
        gt_puts_c("  TCP Baglanti Testi\n", 3);
        gt_puts_c("  DNS: example.com...\n", 0); gt_render();
        uint8_t sip[4]; int dr = dns_resolve("example.com", sip);
        if (dr != 0) { gt_puts_c("  DNS basarisiz, bilinen IP\n", 3); sip[0]=93; sip[1]=184; sip[2]=216; sip[3]=34; }
        char ips[20]; k_itoa(sip[0],ips,10); k_strcpy(ips+k_strlen(ips),"."); k_itoa(sip[1],ips+k_strlen(ips),10); k_strcpy(ips+k_strlen(ips),"."); k_itoa(sip[2],ips+k_strlen(ips),10); k_strcpy(ips+k_strlen(ips),"."); k_itoa(sip[3],ips+k_strlen(ips),10);
        gt_puts_c("  IP: ", 0); gt_puts_c(ips, 1); gt_putc('\n');
        gt_puts_c("  TCP baglaniliyor...\n", 0); gt_render();
        int sock = tcp_connect(sip, 80);
        if (sock < 0) { gt_puts_c("  BASARISIZ! Hata: ", 4); char e2[8]; k_itoa(sock,e2,10); gt_puts_c(e2,4); gt_putc('\n'); return 0; }
        gt_puts_c("  BASARILI!\n", 1);
        const char* req = "GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n";
        tcp_send(sock, req, (uint16_t)k_strlen(req));
        gt_puts_c("  Cevap bekleniyor...\n", 0); gt_render();
        uint8_t resp[512]; int total = 0;
        for (int w = 0; w < 200; w++) { for (volatile int d = 0; d < 200000; d++); net_poll(); int r = tcp_recv(sock, resp+total, (uint16_t)(511-total)); if (r > 0) { total += r; w = 0; } if (!tcp_is_connected(sock) && total > 0) break; }
        resp[total] = 0; tcp_close(sock);
        if (total > 0) { gt_puts_c("  Alindi! (", 1); char rl[8]; k_itoa(total,rl,10); gt_puts_c(rl,1); gt_puts_c(" byte)\n", 1);
            int show = total > 150 ? 150 : total; gt_setcolor(0);
            for (int i = 0; i < show; i++) { char ch = (char)resp[i]; if (ch=='\r') continue; if (ch=='\n') gt_putc('\n'); else if (ch>=32&&ch<127) gt_putc(ch); }
            gt_putc('\n'); gt_puts_c("  TCP TESTI BASARILI!\n", 1);
        } else { gt_puts_c("  Timeout.\n", 4); }
        return 0;
    }

    /* Dosya komutlari */
    if (!k_strcmp(c, "ls")) {
        int count = vfs_count_children(-1);
        gt_puts_c("  /\n", 3);
        for (int i = 0; i < count; i++) {
            int idx = vfs_get_child(-1, i); vfs_node_t* n = vfs_get_node(idx); if (!n) continue;
            gt_puts_c("  ", 0);
            if (n->type == VFS_DIRECTORY) { gt_puts_c(n->name, 6); gt_puts_c("/", 6); }
            else { gt_puts_c(n->name, 5); }
            gt_puts_c("  ", 0);
            if (n->type == VFS_FILE) { char sz[12]; k_itoa((int)n->size,sz,10); gt_puts_c(sz,0); gt_puts_c(" B",0); }
            else { gt_puts_c("<DIR>", 3); }
            gt_putc('\n');
        }
        return 0;
    }

    if (!k_strncmp(c, "ls ", 3)) {
        const char* path = c + 3; while (*path == ' ') path++;
        int dir = vfs_find_path(path);
        if (dir < 0) { gt_puts_c("  Bulunamadi: ", 4); gt_puts_c(path, 5); gt_putc('\n'); return 0; }
        vfs_node_t* dn = vfs_get_node(dir);
        if (!dn || dn->type != VFS_DIRECTORY) { gt_puts_c("  Klasor degil\n", 4); return 0; }
        char dp[VFS_MAX_PATH]; vfs_build_path(dir, dp);
        gt_puts_c("  ", 0); gt_puts_c(dp, 3); gt_putc('\n');
        int count = vfs_count_children(dir);
        for (int i = 0; i < count; i++) {
            int idx = vfs_get_child(dir, i); vfs_node_t* n = vfs_get_node(idx); if (!n) continue;
            gt_puts_c("  ", 0);
            if (n->type == VFS_DIRECTORY) { gt_puts_c(n->name, 6); gt_puts_c("/", 6); }
            else { gt_puts_c(n->name, 5); }
            gt_puts_c("  ", 0);
            if (n->type == VFS_FILE) { char sz[12]; k_itoa((int)n->size,sz,10); gt_puts_c(sz,0); gt_puts_c(" B",0); }
            else { gt_puts_c("<DIR>", 3); }
            gt_putc('\n');
        }
        return 0;
    }

    if (!k_strncmp(c, "cat ", 4)) {
        const char* path = c + 4; while (*path == ' ') path++;
        int node = vfs_find_path(path);
        if (node < 0) { gt_puts_c("  Bulunamadi: ", 4); gt_puts_c(path, 5); gt_putc('\n'); return 0; }
        vfs_node_t* n = vfs_get_node(node);
        if (!n || n->type != VFS_FILE) { gt_puts_c("  Dosya degil\n", 4); return 0; }
        if (!n->data || n->size == 0) { gt_puts_c("  (bos)\n", 0); return 0; }
        gt_setcolor(0);
        for (uint32_t i = 0; i < n->size && i < 1024; i++) {
            char ch = (char)n->data[i];
            if (ch == '\n') gt_putc('\n'); else if (ch >= 32 && ch < 127) gt_putc(ch);
        }
        gt_putc('\n'); return 0;
    }

    if (!k_strncmp(c, "touch ", 6)) {
        const char* name = c + 6; while (*name == ' ') name++;
        if (!k_strlen(name)) { gt_puts_c("  Kullanim: touch ad\n", 3); return 0; }
        int idx = vfs_create_file(name, -1, "", 0);
        gt_puts_c(idx >= 0 ? "  Olusturuldu: " : "  Basarisiz!\n", idx >= 0 ? 1 : 4);
        if (idx >= 0) { gt_puts_c(name, 5); gt_putc('\n'); }
        return 0;
    }

    if (!k_strncmp(c, "mkdir ", 6)) {
        const char* name = c + 6; while (*name == ' ') name++;
        if (!k_strlen(name)) { gt_puts_c("  Kullanim: mkdir ad\n", 3); return 0; }
        int idx = vfs_create_dir(name, -1);
        gt_puts_c(idx >= 0 ? "  Olusturuldu: " : "  Basarisiz!\n", idx >= 0 ? 1 : 4);
        if (idx >= 0) { gt_puts_c(name, 6); gt_putc('\n'); }
        return 0;
    }

    if (!k_strncmp(c, "write ", 6)) {
        const char* rest = c + 6; while (*rest == ' ') rest++;
        char fname[VFS_MAX_NAME]; int fi = 0;
        while (*rest && *rest != ' ' && fi < VFS_MAX_NAME - 1) fname[fi++] = *rest++;
        fname[fi] = '\0'; while (*rest == ' ') rest++;
        int node = vfs_find_path(fname);
        if (node < 0) {
            node = vfs_create_file(fname, -1, rest, k_strlen(rest));
            gt_puts_c(node >= 0 ? "  Yazildi (yeni): " : "  Basarisiz!\n", node >= 0 ? 1 : 4);
            if (node >= 0) { gt_puts_c(fname, 5); gt_putc('\n'); }
        } else {
            int ret = vfs_write_file(node, rest, k_strlen(rest));
            gt_puts_c(ret == 0 ? "  Yazildi: " : "  Basarisiz!\n", ret == 0 ? 1 : 4);
            if (ret == 0) { gt_puts_c(fname, 5); gt_putc('\n'); }
        }
        return 0;
    }

    if (!k_strncmp(c, "rm ", 3)) {
        const char* path = c + 3; while (*path == ' ') path++;
        int node = vfs_find_path(path);
        if (node < 0) { gt_puts_c("  Bulunamadi\n", 4); return 0; }
        int ret = vfs_delete(node);
        if (ret == 0) { gt_puts_c("  Silindi: ", 1); gt_puts_c(path, 5); gt_putc('\n'); }
        else if (ret == -3) { gt_puts_c("  Klasor bos degil!\n", 4); }
        else { gt_puts_c("  Silinemedi!\n", 4); }
        return 0;
    }

    if (!k_strncmp(c, "run ", 4)) {
        const char* path = c + 4; while (*path == ' ') path++;
        if (!k_strlen(path)) { gt_puts_c("  Kullanim: run dosya\n", 3); return 0; }
        gt_puts_c("  Yukleniyor: ", 0); gt_puts_c(path, 2); gt_putc('\n');
        int node = vfs_find_path(path);
        if (node < 0) { gt_puts_c("  Bulunamadi!\n", 4); return 0; }
        vfs_node_t* n = vfs_get_node(node);
        if (!n || !n->data) { gt_puts_c("  Okunamadi!\n", 4); return 0; }
        if (n->size >= 4) {
            uint32_t magic2 = *(uint32_t*)n->data;
            if (magic2 == 0x464C457F) gt_puts_c("  Format: ELF32\n", 2);
            else if (magic2 == 0x4D594F53) gt_puts_c("  Format: MyOS\n", 2);
            else { gt_puts_c("  Bilinmeyen format!\n", 4); return 0; }
        }
        gt_puts_c("  Calistiriliyor...\n", 3); gt_render();
        int ret = elf_load_from_vfs(path);
        if (ret >= 0) { gt_puts_c("  Cikis kodu: ", 1); char rc[12]; k_itoa(ret,rc,10); gt_puts_c(rc,1); gt_putc('\n'); }
        else { gt_puts_c("  HATA: ", 4); char rc2[12]; k_itoa(ret,rc2,10); gt_puts_c(rc2,4); gt_putc('\n'); }
        return 0;
    }

    if (!k_strcmp(c, "elfinfo")) {
        gt_puts_c("  ELF Bilgisi:\n", 3);
        gt_puts_c("  Formatlar: ELF32 i386, MyOS\n", 0);
        gt_puts_c("  Kullanim : run <dosya>\n", 0);
        return 0;
    }

    if (!k_strcmp(c, "syscall")) {
        gt_puts_c("  Syscall Testi:\n", 3);
        char b3[20];
        syscall_result_t r1 = syscall_handle(SYS_MEMINFO, 0, 0, 0, 0);
        gt_puts_c("  MEMINFO(total): ", 0); k_itoa((int)(r1.retval/1024), b3, 10);
        gt_puts_c(b3, 1); gt_puts_c(" KB\n", 0);
        syscall_result_t r2 = syscall_handle(SYS_MEMINFO, 1, 0, 0, 0);
        gt_puts_c("  MEMINFO(used) : ", 0); k_itoa(r2.retval, b3, 10);
        gt_puts_c(b3, 3); gt_puts_c(" byte\n", 0);
        syscall_result_t r3 = syscall_handle(SYS_MALLOC, 512, 0, 0, 0);
        if (r3.retval) {
            gt_puts_c("  MALLOC(512)   : OK\n", 1);
            syscall_handle(SYS_FREE, (uint32_t)r3.retval, 0, 0, 0);
            gt_puts_c("  FREE          : OK\n", 1);
        }
        syscall_result_t r7 = syscall_handle(SYS_TIME, 0, 0, 0, 0);
        int secs = r7.retval; int hh = secs/3600; int mm = (secs%3600)/60; int ss = secs%60;
        char tb[12]; tb[0]='0'+hh/10; tb[1]='0'+hh%10; tb[2]=':'; tb[3]='0'+mm/10; tb[4]='0'+mm%10; tb[5]=':'; tb[6]='0'+ss/10; tb[7]='0'+ss%10; tb[8]=0;
        gt_puts_c("  TIME          : ", 0); gt_puts_c(tb, 2); gt_putc('\n');
        syscall_result_t rw = syscall_handle(SYS_SCREEN_W, 0, 0, 0, 0);
        syscall_result_t rh = syscall_handle(SYS_SCREEN_H, 0, 0, 0, 0);
        gt_puts_c("  SCREEN        : ", 0); k_itoa(rw.retval,b3,10); gt_puts_c(b3,2);
        gt_puts_c("x", 0); k_itoa(rh.retval,b3,10); gt_puts_c(b3,2); gt_putc('\n');
        gt_puts_c("  Toplam: ", 0); k_itoa(SYS_MAX,b3,10); gt_puts_c(b3,2); gt_puts_c(" syscall\n", 0);
        gt_puts_c("  BASARILI!\n", 1);
        return 0;
    }

    /* ======== TASKS KOMUTU ======== */
    if (!k_strcmp(c, "tasks")) {
        gt_puts_c("  Gorev Listesi:\n", 3);
        gt_puts_c("  ID  Ad              Durum       Oncelik   CPU\n", 2);
        gt_puts_c("  --- --------------- ----------- --------- ------\n", 0);
        char b4[20];
        for (int i = 0; i < MAX_TASKS; i++) {
            task_t* t = task_get(i);
            if (!t || t->state == TASK_STATE_FREE) continue;

            gt_puts_c("  ", 0);

            /* ID */
            k_itoa(t->id, b4, 10);
            gt_puts_c(b4, 5);
            int id_len = k_strlen(b4);
            for (int sp = 0; sp < 4 - id_len; sp++) gt_putc(' ');

            /* Ad */
            gt_puts_c(t->name, 5);
            int name_len = k_strlen(t->name);
            for (int sp = 0; sp < 16 - name_len; sp++) gt_putc(' ');

            /* Durum */
            const char* st_str;
            uint8_t st_clr;
            switch (t->state) {
                case TASK_STATE_RUNNING:  st_str = "Calisiyor"; st_clr = 1; break;
                case TASK_STATE_READY:    st_str = "Hazir";     st_clr = 2; break;
                case TASK_STATE_SLEEPING: st_str = "Uyuyor";    st_clr = 3; break;
                case TASK_STATE_BLOCKED:  st_str = "Bloke";     st_clr = 4; break;
                case TASK_STATE_DEAD:     st_str = "Oldu";      st_clr = 0; break;
                default:                  st_str = "?";         st_clr = 0; break;
            }
            gt_puts_c(st_str, st_clr);
            int st_len = k_strlen(st_str);
            for (int sp = 0; sp < 12 - st_len; sp++) gt_putc(' ');

            /* Oncelik */
            const char* pr_str;
            uint8_t pr_clr;
            switch (t->priority) {
                case TASK_PRIORITY_SYSTEM: pr_str = "Sistem"; pr_clr = 4; break;
                case TASK_PRIORITY_HIGH:   pr_str = "Yuksek"; pr_clr = 3; break;
                case TASK_PRIORITY_NORMAL: pr_str = "Normal"; pr_clr = 1; break;
                default:                   pr_str = "Dusuk";  pr_clr = 0; break;
            }
            gt_puts_c(pr_str, pr_clr);
            int pr_len = k_strlen(pr_str);
            for (int sp = 0; sp < 10 - pr_len; sp++) gt_putc(' ');

            /* CPU */
            k_itoa((int)t->cpu_time, b4, 10);
            gt_puts_c(b4, 0);

            gt_putc('\n');
        }
        gt_puts_c("  ---\n", 0);
        gt_puts_c("  Aktif: ", 0);
        k_itoa(task_get_count(), b4, 10);
        gt_puts_c(b4, 1);
        gt_puts_c("  Uptime: ", 0);
        uint32_t up = task_get_ticks() / 100;
        k_itoa((int)(up / 60), b4, 10); gt_puts_c(b4, 2); gt_puts_c("m ", 0);
        k_itoa((int)(up % 60), b4, 10); gt_puts_c(b4, 2); gt_puts_c("s\n", 0);
        return 0;
    }

    if (!k_strcmp(c, "libctest")) {
        gt_puts_c("  === Mini libc Testi ===\n", 3);
        char buf[128];

        /* 1. strlen */
        snprintf(buf, sizeof(buf), "  strlen(\"Hello\") = %d", (int)strlen("Hello"));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 2. strcmp */
        snprintf(buf, sizeof(buf), "  strcmp(\"a\",\"b\") = %d", strcmp("a", "b"));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 3. strchr */
        const char* found = strchr("merhaba", 'h');
        snprintf(buf, sizeof(buf), "  strchr(\"merhaba\",'h') = \"%s\"", found ? found : "NULL");
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 4. strstr */
        const char* sub = strstr("hello world", "world");
        snprintf(buf, sizeof(buf), "  strstr(\"hello world\",\"world\") = \"%s\"", sub ? sub : "NULL");
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 5. atoi */
        snprintf(buf, sizeof(buf), "  atoi(\"-1234\") = %d", atoi("-1234"));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 6. strtol hex */
        snprintf(buf, sizeof(buf), "  strtol(\"0xFF\",16) = %d", (int)strtol("0xFF", 0, 16));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 7. snprintf */
        snprintf(buf, sizeof(buf), "  snprintf: sayi=%d hex=0x%x str=%s", 42, 255, "test");
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 8. malloc/free */
        void* p = malloc(1024);
        if (p) {
            memset(p, 0xAA, 1024);
            gt_puts_c("  malloc(1024)+memset = OK\n", 1);
            free(p);
            gt_puts_c("  free() = OK\n", 1);
        } else {
            gt_puts_c("  malloc = FAIL\n", 4);
        }

        /* 9. strdup */
        char* dup = strdup("kopya");
        if (dup) {
            snprintf(buf, sizeof(buf), "  strdup(\"kopya\") = \"%s\"", dup);
            gt_puts_c(buf, 1); gt_putc('\n');
            free(dup);
        }

        /* 10. fopen/fread/fclose */
        FILE_t* fp = fopen("home/readme.txt", "r");
        if (fp) {
            char fbuf[64];
            char* line = fgets(fbuf, sizeof(fbuf), fp);
            if (line) {
                /* Newline sil */
                int ll = (int)strlen(fbuf);
                if (ll > 0 && fbuf[ll-1] == '\n') fbuf[ll-1] = '\0';
                snprintf(buf, sizeof(buf), "  fopen+fgets = \"%s\"", fbuf);
                gt_puts_c(buf, 1); gt_putc('\n');
            }
            snprintf(buf, sizeof(buf), "  ftell = %d", (int)ftell(fp));
            gt_puts_c(buf, 0); gt_putc('\n');
            fclose(fp);
            gt_puts_c("  fclose = OK\n", 1);
        } else {
            gt_puts_c("  fopen = FAIL\n", 4);
        }

        /* 11. fopen write */
        FILE_t* fp2 = fopen("tmp/test.txt", "w");
        if (fp2) {
            fprintf(fp2, "Merhaba %s! Sayi: %d\n", "Dunya", 2025);
            fclose(fp2);
            gt_puts_c("  fopen(w)+fprintf+fclose = OK\n", 1);

            /* Okuyarak dogrula */
            FILE_t* fp3 = fopen("tmp/test.txt", "r");
            if (fp3) {
                char vbuf[64];
                fgets(vbuf, sizeof(vbuf), fp3);
                int vl = (int)strlen(vbuf);
                if (vl > 0 && vbuf[vl-1] == '\n') vbuf[vl-1] = '\0';
                snprintf(buf, sizeof(buf), "  Dogrulama: \"%s\"", vbuf);
                gt_puts_c(buf, 1); gt_putc('\n');
                fclose(fp3);
            }
        }

        /* 12. isalpha/isdigit */
        snprintf(buf, sizeof(buf), "  isalpha('A')=%d isdigit('5')=%d isspace(' ')=%d",
                 isalpha('A'), isdigit('5'), isspace(' '));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 13. toupper/tolower */
        snprintf(buf, sizeof(buf), "  toupper('a')='%c' tolower('Z')='%c'",
                 toupper('a'), tolower('Z'));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 14. rand */
        snprintf(buf, sizeof(buf), "  rand() = %d, %d, %d",
                 (int)rand_next(), (int)rand_next(), (int)rand_next());
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 15. memcmp */
        snprintf(buf, sizeof(buf), "  memcmp(\"abc\",\"abc\",3) = %d",
                 memcmp("abc", "abc", 3));
        gt_puts_c(buf, 0); gt_putc('\n');

        gt_puts_c("  ===\n", 0);
        gt_puts_c("  LIBC TESTI BASARILI!\n", 1);
        return 0;
    }

    if (!k_strcmp(c, "posixtest")) {
        gt_puts_c("  === POSIX Uyumluluk Testi ===\n", 3);
        char buf[128];

        /* 1. getpid */
        snprintf(buf, sizeof(buf), "  getpid() = %d", getpid());
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 2. getenv */
        char* home = getenv("HOME");
        snprintf(buf, sizeof(buf), "  getenv(HOME) = %s", home ? home : "NULL");
        gt_puts_c(buf, 0); gt_putc('\n');

        char* user = getenv("USER");
        snprintf(buf, sizeof(buf), "  getenv(USER) = %s", user ? user : "NULL");
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 3. setenv */
        setenv("MYVAR", "test123", 1);
        char* myvar = getenv("MYVAR");
        snprintf(buf, sizeof(buf), "  setenv+getenv(MYVAR) = %s", myvar ? myvar : "NULL");
        gt_puts_c(buf, 1); gt_putc('\n');
        unsetenv("MYVAR");

        /* 4. getcwd */
        char cwdbuf[128];
        getcwd(cwdbuf, sizeof(cwdbuf));
        snprintf(buf, sizeof(buf), "  getcwd() = %s", cwdbuf);
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 5. chdir */
        chdir("/sys");
        getcwd(cwdbuf, sizeof(cwdbuf));
        snprintf(buf, sizeof(buf), "  chdir(/sys) -> cwd = %s", cwdbuf);
        gt_puts_c(buf, 0); gt_putc('\n');
        chdir("/home");

        /* 6. access */
        int acc = access("home/readme.txt", F_OK);
        snprintf(buf, sizeof(buf), "  access(readme.txt) = %d", acc);
        gt_puts_c(buf, acc == 0 ? 1 : 4); gt_putc('\n');

        /* 7. opendir/readdir */
        DIR_t* dir = opendir("/");
        if (dir) {
            gt_puts_c("  opendir(/) = OK\n", 1);
            int dc = 0;
            dirent_t* de;
            while ((de = readdir(dir)) != NULL) dc++;
            snprintf(buf, sizeof(buf), "  readdir: %d oge", dc);
            gt_puts_c(buf, 0); gt_putc('\n');
            closedir(dir);
        } else {
            gt_puts_c("  opendir = FAIL\n", 4);
        }

        /* 8. pipe */
        int pfd[2];
        if (pipe(pfd) == 0) {
            gt_puts_c("  pipe() = OK\n", 1);
            const char* msg = "hello pipe";
            pipe_write(pfd[1], msg, strlen(msg));
            char pbuf[32];
            k_memset(pbuf, 0, sizeof(pbuf));
            pipe_read(pfd[0], pbuf, sizeof(pbuf) - 1);
            snprintf(buf, sizeof(buf), "  pipe read = \"%s\"", pbuf);
            gt_puts_c(buf, 1); gt_putc('\n');
        }

        /* 9. signal */
        signal(SIGINT, SIG_IGN);
        gt_puts_c("  signal(SIGINT, SIG_IGN) = OK\n", 1);
        signal(SIGINT, SIG_DFL);

        /* 10. time */
        time_t now;
        time_now(&now);
        tm_t* tm = localtime(&now);
        snprintf(buf, sizeof(buf), "  localtime = %02d:%02d:%02d",
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
        gt_puts_c(buf, 2); gt_putc('\n');

        /* 11. sysconf */
        snprintf(buf, sizeof(buf), "  sysconf(PAGESIZE) = %d", (int)sysconf(_SC_PAGESIZE));
        gt_puts_c(buf, 0); gt_putc('\n');
        snprintf(buf, sizeof(buf), "  sysconf(PHYS_PAGES) = %d", (int)sysconf(_SC_PHYS_PAGES));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 12. errno/strerror */
        snprintf(buf, sizeof(buf), "  strerror(ENOENT) = %s", strerror(ENOENT));
        gt_puts_c(buf, 0); gt_putc('\n');

        /* 13. isatty */
        snprintf(buf, sizeof(buf), "  isatty(0) = %d", isatty(0));
        gt_puts_c(buf, 0); gt_putc('\n');

        gt_puts_c("  ===\n", 0);
        gt_puts_c("  POSIX TESTI BASARILI!\n", 1);
        return 0;
    }

    if (!k_strcmp(c, "env")) {
        gt_puts_c("  Ortam Degiskenleri:\n", 3);
        char buf2[128];
        const char* vars[] = {"HOME","PATH","USER","SHELL","TERM","LANG","HOSTNAME","OS","VERSION","ARCH","PWD"};
        for (int i = 0; i < 11; i++) {
            char* val = getenv(vars[i]);
            snprintf(buf2, sizeof(buf2), "  %s=%s", vars[i], val ? val : "(yok)");
            gt_puts_c(buf2, 0); gt_putc('\n');
        }
        return 0;
    }
    
    if (!k_strncmp(c, "echo ", 5)) { gt_puts_c("  ", 0); gt_puts_c(c+5, 5); gt_putc('\n'); return 0; }
    if (!k_strcmp(c, "reboot")) { outb(0x64, 0xFE); return 0; }

    gt_puts_c("  Bilinmeyen: ", 4); gt_puts_c(c, 5);
    gt_puts_c("\n  'help' yazin.\n", 0);
    return 0;
}

static void gui_terminal(void) {
    gt_clear();
    gt_puts_c("MyOS Terminal v0.3\n", 2);
    gt_puts_c("Cikis: ", 0); gt_puts_c("exit", 1);
    gt_puts_c(" | Yardim: ", 0); gt_puts_c("help\n\n", 1);
    gt_prompt(); gt_render();
    while (1) {
        int c = keyboard_getchar();
        if (c == KEY_ESC) return;
        if (c == KEY_F4 && keyboard_alt_held()) return;
        if (c == '\n') {
            gt_cmd[gt_cmd_len] = 0;
            if (gt_process()) return;
            gt_prompt();
        } else if (c == '\b') {
            if (gt_cmd_len > 0) { gt_cmd_len--; gt_cmd[gt_cmd_len] = 0; gt_putc('\b'); }
        } else if (c >= 32 && c < 127 && gt_cmd_len < 250) {
            gt_cmd[gt_cmd_len++] = (char)c; gt_putc((char)c);
        }
        gt_render();
    }
}

/* ======== MASAUSTU ======== */
typedef struct { int x, y; const char* label; int id; } dicon_t;
static dicon_t dicons[] = {
    {30, 40,  "Bilgisayarim",  1},
    {30, 110, "Terminal",      2},
    {30, 180, "Hakkinda",      3},
    {30, 250, "Ekran Ayar",    4},
    {30, 320, "Dosya Gezgini", 5},
    {30, 390, "Not Defteri",   6},
    {30, 460, "Hesap Mak.",    7},
    {130, 40, "Tarayici",      8},
    {130, 110,"Gorev Yon.",    9},
};
#define DICON_COUNT 9

static void draw_icons(void) {
    for (int i = 0; i < DICON_COUNT; i++) {
        int ix = dicons[i].x, iy = dicons[i].y;
        if (dicons[i].id == 1) draw_icon_computer(ix, iy);
        else if (dicons[i].id == 2) draw_icon_terminal(ix, iy);
        else if (dicons[i].id == 3) draw_icon_info(ix, iy);
        else if (dicons[i].id == 4) draw_icon_display(ix, iy);
        else if (dicons[i].id == 5) { draw_folder_icon(ix+4, iy+4); vesa_fill_rect(ix+4, iy+6, 14, 10, 0xFFCC33); }
        else if (dicons[i].id == 6) { vesa_fill_rect(ix+2,iy+2,20,18,0xEEEEEE); vesa_draw_rect_outline(ix+2,iy+2,20,18,0x888888); vesa_fill_rect(ix+5,iy+6,14,2,0x333333); vesa_fill_rect(ix+5,iy+10,14,2,0x333333); vesa_fill_rect(ix+5,iy+14,10,2,0x333333); }
        else if (dicons[i].id == 7) { vesa_fill_rect(ix+2,iy+2,20,20,0x2D2D44); vesa_draw_rect_outline(ix+2,iy+2,20,20,COLOR_ACCENT); vesa_draw_string(ix+6,iy+6,"1+2",COLOR_TEXT_GREEN,0x2D2D44); }
        else if (dicons[i].id == 8) { vesa_fill_rect(ix+2,iy+2,20,20,0x1E1E30); vesa_draw_rect_outline(ix+2,iy+2,20,20,0x0078D4); vesa_fill_rect(ix+4,iy+4,16,3,0x444466); vesa_fill_rect(ix+4,iy+8,16,12,0x1A1A2E); vesa_fill_rect(ix+6,iy+10,4,2,0x55CCFF); vesa_fill_rect(ix+6,iy+14,8,2,0xAAAAAA); }
        else if (dicons[i].id == 9) { vesa_fill_rect(ix+2,iy+2,20,20,0x1A2A1A); vesa_draw_rect_outline(ix+2,iy+2,20,20,COLOR_TEXT_GREEN); vesa_fill_rect(ix+4,iy+5,16,3,COLOR_TEXT_GREEN); vesa_fill_rect(ix+4,iy+10,12,3,COLOR_TEXT_YELLOW); vesa_fill_rect(ix+4,iy+15,8,3,COLOR_TEXT_CYAN); }
        int tw = k_strlen(dicons[i].label) * 8;
        vesa_draw_string_nobg(ix + 12 - tw/2, iy + 28, dicons[i].label, COLOR_TEXT_WHITE);
    }
}

static void draw_taskbar(void) {
    int sw2 = vesa_get_width(), sh2 = vesa_get_height(), ty = sh2 - 40;
    vesa_fill_rect(0, ty, sw2, 40, COLOR_TASKBAR);
    vesa_fill_rect(0, ty, sw2, 1, COLOR_WINDOW_BORDER);
    uint32_t sc = start_open ? COLOR_START_HOVER : COLOR_START_BTN;
    vesa_fill_rect(4, ty+4, 80, 32, sc);
    draw_windows_logo(12, ty+10, COLOR_TEXT_WHITE);
    vesa_draw_string(32, ty+12, "Baslat", COLOR_TEXT_WHITE, sc);
    char ck[6]; get_time(ck);
    vesa_draw_string(sw2-48, ty+12, ck, COLOR_TEXT_WHITE, COLOR_TASKBAR);
    draw_icon_gear(sw2-75, ty+12, COLOR_TEXT_GREY);
    char dbuf[16]; k_itoa(vesa_get_depth(), dbuf, 10);
    int dl = k_strlen(dbuf); dbuf[dl]='b'; dbuf[dl+1]=' '; dbuf[dl+2]=0;
    int dl2 = k_strlen(dbuf); k_itoa(current_hz, dbuf+dl2, 10);
    int dl3 = k_strlen(dbuf); dbuf[dl3]='H'; dbuf[dl3+1]='z'; dbuf[dl3+2]=0;
    vesa_draw_string(sw2-145, ty+12, dbuf, COLOR_TEXT_CYAN, COLOR_TASKBAR);
    vesa_draw_string(sw2/2-36, ty+12, "MyOS v0.3", COLOR_TEXT_GREY, COLOR_TASKBAR);
}

static void draw_start_menu(int mx2, int my2) {
    int sh2 = vesa_get_height(), smy = sh2 - 40 - 350;
    vesa_fill_rect(0, smy, 240, 350, COLOR_MENU_BG);
    vesa_draw_rect_outline(0, smy, 240, 350, COLOR_WINDOW_BORDER);
    vesa_fill_rect(10, smy+10, 220, 30, COLOR_WINDOW_TITLE);
    draw_windows_logo(18, smy+15, COLOR_ACCENT);
    vesa_draw_string(46, smy+17, "MyOS User", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    const char* items[] = {
        "Terminal", "Dosya Gezgini", "Not Defteri", "Hesap Makinesi",
        "Tarayici", "Gorev Yoneticisi", "Sistem Bilgisi", "Hakkinda",
        "Yeniden Baslat", "Kapat"
    };
    for (int i = 0; i < 10; i++) {
        int iy = smy + 48 + i * 28;
        int h = (mx2 >= 0 && mx2 < 240 && my2 >= iy && my2 < iy + 28);
        uint32_t ibg = h ? COLOR_MENU_HOVER : COLOR_MENU_BG;
        vesa_fill_rect(1, iy, 238, 28, ibg);
        vesa_draw_string(20, iy + 6, items[i], COLOR_TEXT_WHITE, ibg);
        if (i == 9) draw_icon_power(206, iy + 2);
    }
}

static void show_sysinfo(void) {
    monitor_info_t* mi = vesa_get_monitor_info();
    int wx = 80, wy = 10, ww = 640, wh = 570;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Sistem Bilgisi", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    int clx = wx+ww-28, cly = wy+4;
    vesa_fill_rect(clx, cly, 24, 24, COLOR_CLOSE_BTN);
    for (int i=0;i<8;i++) { vesa_putpixel(clx+4+i,cly+4+i,COLOR_TEXT_WHITE); vesa_putpixel(clx+5+i,cly+4+i,COLOR_TEXT_WHITE); vesa_putpixel(clx+11-i,cly+4+i,COLOR_TEXT_WHITE); vesa_putpixel(clx+12-i,cly+4+i,COLOR_TEXT_WHITE); }
    char v[13]; uint32_t eb,ec,ed; __asm__ __volatile__("cpuid":"=b"(eb),"=c"(ec),"=d"(ed):"a"(0));
    *((uint32_t*)&v[0])=eb; *((uint32_t*)&v[4])=ed; *((uint32_t*)&v[8])=ec; v[12]=0;
    int cy = wy + 40;
    vesa_draw_string(wx+15,cy,"CPU:",COLOR_TEXT_GREY,COLOR_WINDOW_BG); vesa_draw_string(wx+120,cy,v,COLOR_TEXT_GREEN,COLOR_WINDOW_BG); cy+=17;
    vesa_draw_string(wx+15,cy,"Adaptor:",COLOR_TEXT_GREY,COLOR_WINDOW_BG); vesa_draw_string(wx+120,cy,mi->adapter_name,COLOR_TEXT_WHITE,COLOR_WINDOW_BG); cy+=17;
    vesa_draw_string(wx+15,cy,"Ortam:",COLOR_TEXT_GREY,COLOR_WINDOW_BG); vesa_draw_string(wx+120,cy,mi->is_virtual?"Sanal Makine":"Fiziksel",mi->is_virtual?COLOR_TEXT_YELLOW:COLOR_TEXT_GREEN,COLOR_WINDOW_BG); cy+=17;
    char vb[12]; k_itoa((int)(mi->vram_bytes/1024/1024),vb,10); int vl2=k_strlen(vb); vb[vl2]=' '; vb[vl2+1]='M'; vb[vl2+2]='B'; vb[vl2+3]=0;
    vesa_draw_string(wx+15,cy,"VRAM:",COLOR_TEXT_GREY,COLOR_WINDOW_BG); vesa_draw_string(wx+120,cy,vb,COLOR_TEXT_CYAN,COLOR_WINDOW_BG); cy+=20;
    /* PCI */
    vesa_fill_rect(wx+10,cy,ww-20,1,COLOR_WINDOW_BORDER); cy+=4;
    vesa_draw_string(wx+15,cy,"PCI:",COLOR_TEXT_CYAN,COLOR_WINDOW_BG); cy+=16;
    int pc = pci_get_device_count();
    for (int i=0;i<pc&&cy<wy+300;i++) {
        pci_device_t* d = pci_get_device(i); if (!d) continue;
        char ln[24]; k_itoa(d->bus,ln,10); k_strcpy(ln+k_strlen(ln),":"); k_itoa(d->slot,ln+k_strlen(ln),10); k_strcpy(ln+k_strlen(ln),"."); k_itoa(d->func,ln+k_strlen(ln),10);
        uint32_t cl = d->class_code==PCI_CLASS_NETWORK ? COLOR_TEXT_GREEN : COLOR_TEXT_GREY;
        vesa_draw_string(wx+20,cy,ln,cl,COLOR_WINDOW_BG); vesa_draw_string(wx+100,cy,pci_vendor_name(d->vendor_id),cl,COLOR_WINDOW_BG); vesa_draw_string(wx+180,cy,pci_class_name(d->class_code,d->subclass),cl,COLOR_WINDOW_BG); cy+=14;
    }
    /* Ag */
    cy+=6; vesa_fill_rect(wx+10,cy,ww-20,1,COLOR_WINDOW_BORDER); cy+=4;
    vesa_draw_string(wx+15,cy,"Ag:",COLOR_TEXT_CYAN,COLOR_WINDOW_BG); cy+=16;
    e1000_info_t* ni = e1000_get_info(); net_config_t* nc = net_get_config();
    if (ni->initialized) {
        char ms2[20]; for(int m=0;m<6;m++){uint8_t hi2=ni->mac[m]>>4,lo2=ni->mac[m]&0x0F;ms2[m*3]=hi2<10?'0'+hi2:'A'+(hi2-10);ms2[m*3+1]=lo2<10?'0'+lo2:'A'+(lo2-10);ms2[m*3+2]=(m<5)?':':'\0';}ms2[17]='\0';
        vesa_draw_string(wx+20,cy,"MAC:",COLOR_TEXT_GREY,COLOR_WINDOW_BG); vesa_draw_string(wx+120,cy,ms2,COLOR_TEXT_GREEN,COLOR_WINDOW_BG); cy+=16;
        vesa_draw_string(wx+20,cy,"Link:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
        if(ni->link_up){char spd[16];k_itoa(ni->speed,spd,10);k_strcpy(spd+k_strlen(spd)," Mbps");vesa_draw_string(wx+120,cy,spd,COLOR_TEXT_GREEN,COLOR_WINDOW_BG);}
        else vesa_draw_string(wx+120,cy,"Bagli Degil",COLOR_TEXT_RED,COLOR_WINDOW_BG);
        cy+=16;
        if(nc->configured){char ips[20];k_itoa(nc->our_ip[0],ips,10);k_strcpy(ips+k_strlen(ips),".");k_itoa(nc->our_ip[1],ips+k_strlen(ips),10);k_strcpy(ips+k_strlen(ips),".");k_itoa(nc->our_ip[2],ips+k_strlen(ips),10);k_strcpy(ips+k_strlen(ips),".");k_itoa(nc->our_ip[3],ips+k_strlen(ips),10);
        vesa_draw_string(wx+20,cy,"IP:",COLOR_TEXT_GREY,COLOR_WINDOW_BG);vesa_draw_string(wx+120,cy,ips,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);cy+=16;}
    }
}

static void show_about(void) {
    int wx=200,wy=120,ww=400,wh=260;
    vesa_fill_rect(wx,wy,ww,wh,COLOR_WINDOW_BG); vesa_fill_rect(wx,wy,ww,32,COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx,wy,ww,wh,COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16,wy+8,"Hakkinda",COLOR_TEXT_WHITE,COLOR_WINDOW_TITLE);
    int clx=wx+ww-28,cly=wy+4; vesa_fill_rect(clx,cly,24,24,COLOR_CLOSE_BTN);
    for(int i=0;i<8;i++){vesa_putpixel(clx+4+i,cly+4+i,COLOR_TEXT_WHITE);vesa_putpixel(clx+5+i,cly+4+i,COLOR_TEXT_WHITE);vesa_putpixel(clx+11-i,cly+4+i,COLOR_TEXT_WHITE);vesa_putpixel(clx+12-i,cly+4+i,COLOR_TEXT_WHITE);}
    draw_windows_logo(wx+ww/2-8,wy+50,COLOR_ACCENT);
    vesa_draw_string(wx+ww/2-28,wy+80,"MyOS",COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2-52,wy+100,"Versiyon 0.3.0",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+140,"Sifirdan yazilmis isletim sistemi",COLOR_TEXT_CYAN,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+170,"x86 32-bit | VESA Grafik",COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    vesa_draw_string(wx+40,wy+200,"Gercek donanim algilama aktif",COLOR_TEXT_GREEN,COLOR_WINDOW_BG);
}

/* Hata popup */
static char err_title[32], err_l1[64], err_l2[64], err_l3[64];
static void show_error(int mx2, int my2) {
    int pw=400,ph=240,px=vesa_get_width()/2-pw/2,py=vesa_get_height()/2-ph/2;
    uint32_t*bb=backbuffer;int sw2=vesa_get_width(),sh2=vesa_get_height();
    for(int y=0;y<sh2;y++)for(int x=0;x<sw2;x++)if(x<px||x>=px+pw||y<py||y>=py+ph){uint32_t o=bb[y*sw2+x];bb[y*sw2+x]=(((o>>16)&0xFF)/3<<16)|(((o>>8)&0xFF)/3<<8)|((o&0xFF)/3);}
    vesa_fill_rect(px,py,pw,ph,COLOR_WINDOW_BG);vesa_fill_rect(px,py,pw,32,0x881122);vesa_draw_rect_outline(px,py,pw,ph,0xAA2233);
    vesa_draw_string(px+16,py+8,err_title,COLOR_TEXT_WHITE,0x881122);
    int icx=px+pw/2,icy=py+72;for(int dy=-18;dy<=18;dy++)for(int dx=-18;dx<=18;dx++){int d=dx*dx+dy*dy;if(d<=324&&d>=256)vesa_putpixel_raw(icx+dx,icy+dy,COLOR_TEXT_RED);}
    vesa_fill_rect(icx-2,icy-10,4,12,COLOR_TEXT_RED);vesa_fill_rect(icx-2,icy+5,4,4,COLOR_TEXT_RED);
    vesa_draw_string(px+30,py+105,err_l1,COLOR_TEXT_WHITE,COLOR_WINDOW_BG);
    vesa_draw_string(px+30,py+125,err_l2,COLOR_TEXT_YELLOW,COLOR_WINDOW_BG);
    vesa_draw_string(px+30,py+145,err_l3,COLOR_TEXT_GREY,COLOR_WINDOW_BG);
    int obx=px+pw/2-60,oby=py+ph-45;int oh=(mx2>=obx&&mx2<obx+120&&my2>=oby&&my2<oby+32);
    vesa_fill_rect(obx,oby,120,32,oh?COLOR_START_HOVER:COLOR_ACCENT);vesa_draw_rect_outline(obx,oby,120,32,0x005599);
    vesa_draw_string(obx+36,oby+8,"Tamam",COLOR_TEXT_WHITE,oh?COLOR_START_HOVER:COLOR_ACCENT);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);vesa_draw_string(320,290,"Kapatiliyor...",COLOR_TEXT_WHITE,COLOR_BLACK);vesa_copy_buffer();
    outw(0x604,0x2000);outw(0xB004,0x2000);outw(0x4004,0x3400);__asm__ __volatile__("cli;hlt");
}

/* ======== ANA DONGU ======== */
void gui_run(void) {
    mouse_state_t ms;
    int sh2 = vesa_get_height(), sw2 = vesa_get_width();
    vesa_detect_monitor();
    monitor_info_t* mi = vesa_get_monitor_info();
    settings_selected_depth = vesa_get_depth();
    selected_hz = current_hz;
    mi->adapter_id = 0; outw(0x01CE, 0); mi->adapter_id = inw(0x01CF);

    /* Arka plan gorevleri */
    task_create("net_poll", 0, TASK_PRIORITY_SYSTEM);
    task_create("gui", 0, TASK_PRIORITY_HIGH);

    while (1) {
        mouse_poll(&ms);
        int key = keyboard_poll();
        net_poll();
        task_schedule();

        if (key == KEY_F4 && keyboard_alt_held()) {
            if (window_open) { window_open = 0; continue; }
            do_shutdown();
        }

        vesa_fill_screen(COLOR_BG);
        draw_icons();
        if (window_open == 1) show_sysinfo();
        if (window_open == 2) show_about();
        if (window_open == 5 || window_open == 6) show_error(ms.x, ms.y);
        draw_taskbar();
        if (start_open) draw_start_menu(ms.x, ms.y);
        mouse_draw_cursor(ms.x, ms.y);
        vesa_copy_buffer();

        if (!ms.click) continue;

        /* Hata popup */
        if (window_open == 5 || window_open == 6) {
            int pw=400,ph=240,px2=sw2/2-pw/2,py2=sh2/2-ph/2;
            if ((ms.x>=px2+pw-24&&ms.x<px2+pw-8&&ms.y>=py2+8&&ms.y<py2+24)||(ms.x>=px2+pw/2-60&&ms.x<px2+pw/2+60&&ms.y>=py2+ph-45&&ms.y<py2+ph-13))
                window_open = 0;
            continue;
        }

        /* Pencere kapat */
        if (window_open == 1) {
            int wx=80,wy=10,ww=640;
            if (ms.x>=wx+ww-28&&ms.x<wx+ww-4&&ms.y>=wy+4&&ms.y<wy+28) { window_open=0; continue; }
        }
        if (window_open == 2) {
            int wx=200,wy=120,ww=400;
            if (ms.x>=wx+ww-28&&ms.x<wx+ww-4&&ms.y>=wy+4&&ms.y<wy+28) { window_open=0; continue; }
        }

        /* Baslat */
        if (ms.x>=4&&ms.x<84&&ms.y>=sh2-36&&ms.y<sh2-4) { start_open=!start_open; window_open=0; continue; }

        /* Gear */
        if (ms.x>=sw2-85&&ms.x<sw2-55&&ms.y>=sh2-32&&ms.y<sh2-8) { window_open=3; settings_selected_depth=vesa_get_depth(); start_open=0; continue; }

        /* Baslat menu */
        if (start_open) {
            int smy = sh2-40-350;
            for (int i = 0; i < 10; i++) {
                int iy = smy+48+i*28;
                if (ms.x>=0&&ms.x<240&&ms.y>=iy&&ms.y<iy+28) {
                    start_open = 0;
                    if (i==0) gui_terminal();
                    else if (i==1) app_file_explorer();
                    else if (i==2) app_notepad();
                    else if (i==3) app_calculator();
                    else if (i==4) app_browser();
                    else if (i==5) app_task_manager();
                    else if (i==6) { window_open=1; }
                    else if (i==7) { window_open=2; }
                    else if (i==8) outb(0x64,0xFE);
                    else if (i==9) do_shutdown();
                    break;
                }
            }
            if (ms.x>240||ms.y<(sh2-40-350)) start_open=0;
            continue;
        }

        /* Masaustu ikonlari */
        for (int i = 0; i < DICON_COUNT; i++) {
            int ix=dicons[i].x, iy=dicons[i].y;
            if (ms.x>=ix&&ms.x<ix+60&&ms.y>=iy&&ms.y<iy+50) {
                if (dicons[i].id==1) window_open=1;
                else if (dicons[i].id==2) gui_terminal();
                else if (dicons[i].id==3) window_open=2;
                else if (dicons[i].id==4) { window_open=3; settings_selected_depth=vesa_get_depth(); }
                else if (dicons[i].id==5) app_file_explorer();
                else if (dicons[i].id==6) app_notepad();
                else if (dicons[i].id==7) app_calculator();
                else if (dicons[i].id==8) app_browser();
                else if (dicons[i].id==9) app_task_manager();
                start_open=0;
                break;
            }
        }
    }
}
