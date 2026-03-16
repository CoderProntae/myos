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
    vga_print_colored("  echo     ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Yazi yazdir\n");
    vga_print_colored("  time     ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Saat\n");
    vga_print_colored("  sysinfo  ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Sistem bilgisi\n");
    vga_print_colored("  exit     ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Masaustune don\n");
    vga_print_colored("  reboot   ", VGA_LIGHT_CYAN, VGA_BLACK); vga_print("- Yeniden baslat\n\n");
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

static int process_command(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (k_strlen(cmd) == 0)            { vga_print("\n"); return 0; }
    if (k_strcmp(cmd, "help") == 0)     { cmd_help(); return 0; }
    if (k_strcmp(cmd, "clear") == 0)    { vga_clear(); return 0; }
    if (k_strcmp(cmd, "cls") == 0)      { vga_clear(); return 0; }
    if (k_strcmp(cmd, "time") == 0)     { cmd_time(); return 0; }
    if (k_strcmp(cmd, "exit") == 0)     { return 1; }
    if (k_strcmp(cmd, "reboot") == 0)   { outb(0x64, 0xFE); return 0; }
    if (k_strncmp(cmd, "echo ", 5) == 0) {
        vga_print("\n  ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(cmd + 5);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("\n\n"); return 0;
    }
    vga_print("\n  ");
    vga_print_colored("Bilinmeyen: ", VGA_LIGHT_RED, VGA_BLACK);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(cmd);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\n  'help' yazin.\n\n");
    return 0;
}

void shell_run(void) {
    cmd_pos = 0;
    k_memset(cmd_buffer, 0, MAX_CMD);
    print_prompt();
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            cmd_buffer[cmd_pos] = '\0';
            int should_exit = process_command(cmd_buffer);
            if (should_exit) return;
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
