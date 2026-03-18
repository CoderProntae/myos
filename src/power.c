#include "power.h"
#include "kernel.h"

/*
 * MyOS Power Management
 * Shutdown, Reboot, Halt fonksiyonları
 */

void shutdown(void) {
    // Ekrana mesaj yaz
    print("\n", 0x0F);
    print("  ____  _           _   _                 \n", 0x0B);
    print(" / ___|| |__  _   _| |_| |_ ___  _ __ ___ \n", 0x0B);
    print(" \\___ \\| '_ \\| | | | __| __/ _ \\| '__/ _ \\\n", 0x0B);
    print("  ___) | | | | |_| | |_| || (_) | | |  __/\n", 0x0B);
    print(" |____/|_| |_|\\__,_|\\__|\\__\\___/|_|  \\___|\n", 0x0B);
    print("\n", 0x0F);
    print("  Sistem guvenli bir sekilde kapatiliyor...\n\n", 0x0A);
    
    // Kısa bekleme (kullanıcı görsün)
    for (volatile int i = 0; i < 50000000; i++);
    
    // ===== QEMU =====
    // QEMU -device isa-debug-exit ile
    outw(0x604, 0x2000);
    
    // ===== QEMU (eski versiyon) =====
    outw(0x4004, 0x3400);
    
    // ===== Bochs =====
    outw(0xB004, 0x2000);
    
    // ===== VirtualBox =====
    // ACPI shutdown (VirtualBox için çalışır)
    outw(0x4004, 0x3400);
    
    // ===== Gerçek donanım için ACPI =====
    // Not: Tam ACPI desteği için ACPI tablolarını parse etmek gerekir
    // Bu ileride eklenecek (Phase 1.9)
    
    // Hiçbiri çalışmazsa CPU'yu durdur
    print("  [!] Otomatik kapatma basarisiz.\n", 0x0C);
    print("  [!] Lutfen bilgisayari manuel kapatiniz.\n", 0x0C);
    
    halt();
}

void reboot(void) {
    print("\n", 0x0F);
    print("  _____      _                 _   \n", 0x0E);
    print(" |  __ \\    | |               | |  \n", 0x0E);
    print(" | |__) |___| |__   ___   ___ | |_ \n", 0x0E);
    print(" |  _  // _ \\ '_ \\ / _ \\ / _ \\| __|\n", 0x0E);
    print(" | | \\ \\  __/ |_) | (_) | (_) | |_ \n", 0x0E);
    print(" |_|  \\_\\___|_.__/ \\___/ \\___/ \\__|\n", 0x0E);
    print("\n", 0x0F);
    print("  Sistem yeniden baslatiliyor...\n\n", 0x0A);
    
    // Kısa bekleme
    for (volatile int i = 0; i < 30000000; i++);
    
    // ===== Yöntem 1: 8042 Keyboard Controller =====
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    
    // ===== Yöntem 2: Triple Fault =====
    // IDT'yi sıfırla ve interrupt oluştur
    // lidt ile boş IDT yükle
    
    // Hiçbiri çalışmazsa
    print("  [!] Otomatik yeniden baslatma basarisiz.\n", 0x0C);
    print("  [!] Lutfen bilgisayari manuel yeniden baslatin.\n", 0x0C);
    
    halt();
}

void halt(void) {
    // Interrupt'ları kapat ve CPU'yu durdur
    asm volatile ("cli");
    while (1) {
        asm volatile ("hlt");
    }
}
