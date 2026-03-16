#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "io.h"

#define MAX_CMD 256
static char cmd_buffer[MAX_CMD];
static int cmd_pos;

static void print_prompt(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("myos");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(":");
    vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    vga_print("~");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("$ ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static void cmd_help(void) {
    vga_print("\n");
    vga_print_colored(" === MyOS Komutlari ===\n\n", VGA_YELLOW, VGA_BLACK);
    vga_print_colored("  help     ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Yardim\n");
    vga_print_colored("  clear    ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Ekrani temizle\n");
    vga_print_colored("  about    ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Hakkinda\n");
    vga_print_colored("  echo     ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Yazi yazdir\n");
    vga_print_colored("  color    ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Renk paleti\n");
    vga_print_colored("  time     ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Saat\n");
    vga_print_colored("  sysinfo  ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Sistem bilgisi\n");
    vga_print_colored("  gui      ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- GUI moduna gec\n");
    vga_print_colored("  reboot   ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Yeniden baslat\n");
    vga_print_colored("  shutdown ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Kapat\n\n");
}

static void cmd_about(void) {
    vga_print("\n");
    vga_print_colored("  MyOS v0.2\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  x86 32-bit / C + Assembly\n");
    vga_print("  GUI + Terminal modlari\n\n");
}

static void cmd_color(void) {
    const char* names[] = {
        "BLACK ","BLUE  ","GREEN ","CYAN  ",
        "RED   ","MGENTA","BROWN ","LGREY ",
        "DGREY ","LBLUE ","LGREEN","LCYAN ",
        "LRED  ","LMGNT ","YELLOW","WHITE "
    };
    vga_print("\n  Renk Paleti:\n\n");
    for (int i = 0; i < 16; i++) {
        vga_print("  ");
        vga_print_colored("  ", (uint8_t)i, (uint8_t)i);
        vga_print(" ");
        vga_set_color((uint8_t)i, VGA_BLACK);
        vga_print(names[i]);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        if (i % 4 == 3) vga_print("\n");
    }
    vga_print("\n");
}

static void cmd_time(void) {
    outb(0x70, 0x04); uint8_t h = bcd_to_bin(inb(0x71));
    outb(0x70, 0x02); uint8_t m = bcd_to_bin(inb(0x71));
    outb(0x70, 0x00); uint8_t s = bcd_to_bin(inb(0x71));
    char buf[9];
    buf[0]='0'+h/10; buf[1]='0'+h%10; buf[2]=':';
    buf[3]='0'+m/10; buf[4]='0'+m%10; buf[5]=':';
    buf[6]='0'+s/10; buf[7]='0'+s%10; buf[8]='\0';
    vga_print("\n  Saat: ");
    vga_print_colored(buf, VGA_YELLOW, VGA_BLACK);
    vga_print(" (UTC)\n\n");
}

static void cmd_sysinfo(void) {
    char vendor[13];
    uint32_t ebx, ecx, edx;
    __asm__ __volatile__("cpuid" : "=b"(ebx),"=c"(ecx),"=d"(edx) : "a"(0));
    *((uint32_t*)&vendor[0]) = ebx;
    *((uint32_t*)&vendor[4]) = edx;
    *((uint32_t*)&vendor[8]) = ecx;
    vendor[12] = '\0';
    vga_print("\n");
    vga_print_colored("  [Sistem Bilgisi]\n\n", VGA_YELLOW, VGA_BLACK);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  CPU    : "); vga_print_colored(vendor, VGA_LIGHT_GREEN, VGA_BLACK); vga_print("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Mimari : i686 32-bit\n");
    vga_print("  VGA    : 80x25 text\n");
    vga_print("  Giris  : PS/2 klavye + mouse\n\n");
}

static int process_command(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (k_strlen(cmd) == 0) { vga_print("\n"); return SHELL_EXIT_NONE; }

    if (k_strcmp(cmd, "help") == 0)     { cmd_help(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "clear") == 0)    { vga_clear(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "cls") == 0)      { vga_clear(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "about") == 0)    { cmd_about(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "color") == 0)    { cmd_color(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "time") == 0)     { cmd_time(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "sysinfo") == 0)  { cmd_sysinfo(); return SHELL_EXIT_NONE; }
    if (k_strcmp(cmd, "gui") == 0)      { return SHELL_EXIT_GUI; }

    if (k_strcmp(cmd, "reboot") == 0) {
        outb(0x64, 0xFE);
        return SHELL_EXIT_NONE;
    }

    if (k_strcmp(cmd, "shutdown") == 0) {
        vga_print_colored("\n  Kapatiliyor...\n", VGA_YELLOW, VGA_BLACK);
        outw(0x604,  0x2000);
        outw(0xB004, 0x2000);
        outw(0x4004, 0x3400);
        __asm__ __volatile__("cli; hlt");
        return SHELL_EXIT_NONE;
    }

    if (k_strncmp(cmd, "echo ", 5) == 0) {
        vga_print("\n  ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(cmd + 5);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n\n");
        return SHELL_EXIT_NONE;
    }

    vga_print("\n  ");
    vga_print_colored("Bilinmeyen: ", VGA_LIGHT_RED, VGA_BLACK);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(cmd);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n  'help' yazin.\n\n");
    return SHELL_EXIT_NONE;
}

int shell_run(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_show_cursor();
    vga_print_colored("\n  === MyOS Terminal ===\n", VGA_YELLOW, VGA_BLACK);
    vga_print("  'gui' yazarak GUI moduna donebilirsiniz.\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    cmd_pos = 0;
    k_memset(cmd_buffer, 0, MAX_CMD);
    print_prompt();

    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            cmd_buffer[cmd_pos] = '\0';
            int r = process_command(cmd_buffer);
            if (r == SHELL_EXIT_GUI) return SHELL_EXIT_GUI;
            cmd_pos = 0;
            k_memset(cmd_buffer, 0, MAX_CMD);
            print_prompt();
        } else if (c == '\b') {
            if (cmd_pos > 0) { cmd_pos--; cmd_buffer[cmd_pos] = '\0'; vga_backspace(); }
        } else if (c >= 32 && c < 127 && cmd_pos < MAX_CMD - 1) {
            cmd_buffer[cmd_pos++] = c;
            vga_putchar(c);
        }
    }
}
