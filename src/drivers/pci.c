#include "pci.h"
#include "io.h"
#include "string.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count = 0;

/* ======== PCI Config Space Erisim ======== */

static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)(
        ((uint32_t)1 << 31) |          /* Enable bit */
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        ((uint32_t)(offset & 0xFC))
    );
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outw(PCI_CONFIG_ADDR, (uint16_t)(pci_addr(bus, slot, func, offset) & 0xFFFF));
    outw(PCI_CONFIG_ADDR + 2, (uint16_t)(pci_addr(bus, slot, func, offset) >> 16));
    return ((uint32_t)inw(PCI_CONFIG_DATA + (offset & 2))) |
           ((uint32_t)inw(PCI_CONFIG_DATA + (offset & 2) + 2) << 16);
}

/* Duzgun 32-bit port I/O — outl/inl */
static void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

static uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = pci_addr(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = pci_addr(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    return (uint16_t)(val >> ((offset & 2) * 8));
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    pci_cfg_write32(bus, slot, func, offset, value);
}

/* ======== PCI Bus Mastering ======== */

void pci_enable_bus_mastering(pci_device_t* dev) {
    uint32_t cmd = pci_cfg_read32(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= (1 << 2);  /* Bus Master Enable */
    cmd |= (1 << 1);  /* Memory Space Enable */
    cmd |= (1 << 0);  /* I/O Space Enable */
    pci_cfg_write32(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

/* ======== Cihaz Tara ======== */

static void pci_scan_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t reg0 = pci_cfg_read32(bus, slot, func, 0x00);
    uint16_t vendor = (uint16_t)(reg0 & 0xFFFF);
    uint16_t device = (uint16_t)(reg0 >> 16);

    /* Cihaz yok */
    if (vendor == 0xFFFF) return;
    if (device_count >= PCI_MAX_DEVICES) return;

    pci_device_t* dev = &devices[device_count];
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = vendor;
    dev->device_id = device;

    /* Sinif bilgisi */
    uint32_t reg2 = pci_cfg_read32(bus, slot, func, 0x08);
    dev->class_code = (uint8_t)(reg2 >> 24);
    dev->subclass   = (uint8_t)(reg2 >> 16);
    dev->prog_if    = (uint8_t)(reg2 >> 8);

    /* Header type */
    uint32_t reg3 = pci_cfg_read32(bus, slot, func, 0x0C);
    dev->header_type = (uint8_t)(reg3 >> 16);

    /* BAR'lar */
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_cfg_read32(bus, slot, func, 0x10 + i * 4);
    }

    /* IRQ */
    uint32_t irq_reg = pci_cfg_read32(bus, slot, func, 0x3C);
    dev->irq = (uint8_t)(irq_reg & 0xFF);

    device_count++;
}

/* ======== Init ======== */

void pci_init(void) {
    device_count = 0;

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            /* Cihaz var mi kontrol */
            uint32_t reg0 = pci_cfg_read32((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(reg0 & 0xFFFF);
            if (vendor == 0xFFFF) continue;

            /* Fonksiyon 0 */
            pci_scan_device((uint8_t)bus, (uint8_t)slot, 0);

            /* Multi-function cihaz mi? */
            uint32_t reg3 = pci_cfg_read32((uint8_t)bus, (uint8_t)slot, 0, 0x0C);
            uint8_t header = (uint8_t)(reg3 >> 16);
            if (header & 0x80) {
                /* Diger fonksiyonlari da tara */
                for (int func = 1; func < 8; func++) {
                    pci_scan_device((uint8_t)bus, (uint8_t)slot, (uint8_t)func);
                }
            }
        }
    }
}

/* ======== Arama ======== */

int pci_get_device_count(void) {
    return device_count;
}

pci_device_t* pci_get_device(int index) {
    if (index >= 0 && index < device_count)
        return &devices[index];
    return 0;
}

pci_device_t* pci_find_device(uint16_t vendor, uint16_t device) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor && devices[i].device_id == device)
            return &devices[i];
    }
    return 0;
}

pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].class_code == class_code && devices[i].subclass == subclass)
            return &devices[i];
    }
    return 0;
}

/* ======== Isim Tablolari ======== */

const char* pci_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel";
        case 0x1022: return "AMD";
        case 0x10DE: return "NVIDIA";
        case 0x10EC: return "Realtek";
        case 0x1AF4: return "VirtIO";
        case 0x15AD: return "VMware";
        case 0x80EE: return "VirtualBox";
        case 0x1234: return "Bochs/QEMU";
        case 0x1B36: return "QEMU";
        case 0x1013: return "Cirrus Logic";
        case 0x5333: return "S3 Graphics";
        default:     return "Bilinmeyen";
    }
}

const char* pci_class_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case 0x00:
            return "Siniflandirilmamis";
        case 0x01:
            switch (subclass) {
                case 0x00: return "SCSI Denetleyici";
                case 0x01: return "IDE Denetleyici";
                case 0x05: return "ATA Denetleyici";
                case 0x06: return "SATA Denetleyici";
                case 0x08: return "NVMe Denetleyici";
                default:   return "Depolama";
            }
        case 0x02:
            switch (subclass) {
                case 0x00: return "Ethernet";
                case 0x80: return "Ag (Diger)";
                default:   return "Ag Cihazi";
            }
        case 0x03:
            switch (subclass) {
                case 0x00: return "VGA Uyumlu";
                case 0x01: return "XGA";
                case 0x02: return "3D";
                default:   return "Goruntu";
            }
        case 0x04: return "Multimedya";
        case 0x05: return "Bellek Denetleyici";
        case 0x06:
            switch (subclass) {
                case 0x00: return "Host Bridge";
                case 0x01: return "ISA Bridge";
                case 0x04: return "PCI-PCI Bridge";
                case 0x80: return "Bridge (Diger)";
                default:   return "Bridge";
            }
        case 0x07: return "Iletisim";
        case 0x08: return "Sistem Cevre Birimi";
        case 0x09: return "Giris Cihazi";
        case 0x0C:
            switch (subclass) {
                case 0x03: return "USB Denetleyici";
                case 0x05: return "SMBus";
                default:   return "Seri Bus";
            }
        case 0x0D: return "Kablosuz";
        default:   return "Diger";
    }
}
