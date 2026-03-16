#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/* PCI cihaz bilgisi */
typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint32_t bar[6];       /* Base Address Registers */
    uint8_t  irq;
} pci_device_t;

#define PCI_MAX_DEVICES 32

/* Bilinen vendor ID'ler */
#define PCI_VENDOR_INTEL    0x8086
#define PCI_VENDOR_AMD      0x1022
#define PCI_VENDOR_REALTEK  0x10EC
#define PCI_VENDOR_VIRTIO   0x1AF4
#define PCI_VENDOR_VMWARE   0x15AD
#define PCI_VENDOR_INNOTEK  0x80EE  /* VirtualBox */

/* Bilinen device ID'ler */
#define PCI_DEVICE_E1000    0x100E  /* Intel 82540EM (VirtualBox varsayilan) */
#define PCI_DEVICE_E1000_2  0x100F
#define PCI_DEVICE_E1000_3  0x10D3
#define PCI_DEVICE_E1000E   0x10EA
#define PCI_DEVICE_RTL8139  0x8139

/* PCI sinif kodlari */
#define PCI_CLASS_NETWORK   0x02
#define PCI_CLASS_DISPLAY   0x03
#define PCI_CLASS_STORAGE   0x01
#define PCI_CLASS_BRIDGE    0x06
#define PCI_CLASS_SERIAL    0x0C

/* Fonksiyonlar */
void          pci_init(void);
int           pci_get_device_count(void);
pci_device_t* pci_get_device(int index);
pci_device_t* pci_find_device(uint16_t vendor, uint16_t device);
pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass);
uint32_t      pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t      pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void          pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void          pci_enable_bus_mastering(pci_device_t* dev);
const char*   pci_class_name(uint8_t class_code, uint8_t subclass);
const char*   pci_vendor_name(uint16_t vendor_id);

#endif
