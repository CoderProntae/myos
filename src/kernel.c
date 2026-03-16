#include "vga.h"
#include "shell.h"

void kernel_main(void) {
    vga_init();
    vga_clear();
    vga_print("\n\n");
    vga_print_colored("  __  __        ___  ____  \n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_colored(" |  \\/  |_   _ / _ \\/ ___| \n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_colored(" | |\\/| | | | | | | \\___ \\ \n", VGA_LIGHT_BLUE, VGA_BLACK);
    vga_print_colored(" | |  | | |_| | |_| |___) |\n", VGA_BLUE, VGA_BLACK);
    vga_print_colored(" |_|  |_|\\__, |\\___/|____/ \n", VGA_BLUE, VGA_BLACK);
    vga_print_colored("         |___/              \n", VGA_BLUE, VGA_BLACK);
    vga_print("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Hos Geldiniz! Komutlar icin 'help' yazin.\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    shell_run();
}
