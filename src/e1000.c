#include "e1000.h"
#include "pci.h"
#include "io.h"
#include "string.h"

/* MMIO base adresi */
static volatile uint32_t* mmio_base = 0;
static uint32_t mmio_addr = 0;

/* Descriptor ring'ler — 16-byte aligned */
static e1000_rx_desc_t rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(16)));

/* Paket bufferlari */
static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_TX_BUF_SIZE] __attribute__((aligned(16)));

/* Mevcut indeksler */
static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;

/* Kart bilgisi */
static e1000_info_t info;

/* ======== MMIO Okuma/Yazma ======== */

static void e1000_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)((uint8_t*)mmio_base + reg) = value;
}

static uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t*)((uint8_t*)mmio_base + reg);
}

/* ======== EEPROM Okuma ======== */

static uint16_t e1000_eeprom_read(uint8_t addr) {
    uint32_t val;

    /* EEPROM okuma komutu gonder */
    e1000_write(E1000_EERD, ((uint32_t)addr << 8) | 1);

    /* Tamamlanmasini bekle */
    int timeout = 1000;
    while (timeout-- > 0) {
        val = e1000_read(E1000_EERD);
        if (val & (1 << 4))   /* Done bit */
            return (uint16_t)(val >> 16);
        for (volatile int i = 0; i < 1000; i++);
    }

    return 0;
}

/* ======== MAC Adresi Oku ======== */

static void e1000_read_mac(void) {
    uint16_t mac01 = e1000_eeprom_read(0);
    uint16_t mac23 = e1000_eeprom_read(1);
    uint16_t mac45 = e1000_eeprom_read(2);

    info.mac[0] = (uint8_t)(mac01 & 0xFF);
    info.mac[1] = (uint8_t)(mac01 >> 8);
    info.mac[2] = (uint8_t)(mac23 & 0xFF);
    info.mac[3] = (uint8_t)(mac23 >> 8);
    info.mac[4] = (uint8_t)(mac45 & 0xFF);
    info.mac[5] = (uint8_t)(mac45 >> 8);

    /* MAC adresini donanim filtresine yaz */
    uint32_t ral = (uint32_t)info.mac[0] |
                   ((uint32_t)info.mac[1] << 8) |
                   ((uint32_t)info.mac[2] << 16) |
                   ((uint32_t)info.mac[3] << 24);
    uint32_t rah = (uint32_t)info.mac[4] |
                   ((uint32_t)info.mac[5] << 8) |
                   (1u << 31);  /* Address Valid */

    e1000_write(E1000_RAL, ral);
    e1000_write(E1000_RAH, rah);
}

/* ======== RX Baslatma ======== */

static void e1000_init_rx(void) {
    /* Descriptor'lari hazirla */
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr    = (uint64_t)(uint32_t)&rx_buffers[i][0];
        rx_descs[i].length  = 0;
        rx_descs[i].status  = 0;
        rx_descs[i].errors  = 0;
        rx_descs[i].special = 0;
    }

    /* Ring buffer adresini karta bildir */
    e1000_write(E1000_RDBAL, (uint32_t)&rx_descs[0]);
    e1000_write(E1000_RDBAH, 0);

    /* Ring boyutu (byte cinsinden) */
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));

    /* Head ve Tail */
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);

    rx_cur = 0;

    /* RX kontrol: enable + broadcast + 2048 byte buffer + strip CRC */
    e1000_write(E1000_RCTL,
        E1000_RCTL_EN |
        E1000_RCTL_BAM |
        E1000_RCTL_BSIZE_2048 |
        E1000_RCTL_SECRC);
}

/* ======== TX Baslatma ======== */

static void e1000_init_tx(void) {
    /* Descriptor'lari hazirla */
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr    = (uint64_t)(uint32_t)&tx_buffers[i][0];
        tx_descs[i].length  = 0;
        tx_descs[i].cmd     = 0;
        tx_descs[i].status  = E1000_TXD_STAT_DD;  /* Bos/kullanilabilir */
        tx_descs[i].cso     = 0;
        tx_descs[i].css     = 0;
        tx_descs[i].special = 0;
    }

    /* Ring buffer adresi */
    e1000_write(E1000_TDBAL, (uint32_t)&tx_descs[0]);
    e1000_write(E1000_TDBAH, 0);

    /* Ring boyutu */
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));

    /* Head ve Tail */
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    tx_cur = 0;

    /* Inter-Packet Gap (zorunlu) */
    e1000_write(E1000_TIPG, (10) | (10 << 10) | (10 << 20));

    /* TX kontrol: enable + pad short packets */
    e1000_write(E1000_TCTL,
        E1000_TCTL_EN |
        E1000_TCTL_PSP |
        (15 << 4) |      /* Collision Threshold */
        (64 << 12));      /* Collision Distance */
}

/* ======== Multicast Tablosu Temizle ======== */

static void e1000_clear_multicast(void) {
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + i * 4, 0);
    }
}

/* ======== Link Durumu ======== */

static void e1000_check_link(void) {
    uint32_t status = e1000_read(E1000_STATUS);
    info.link_up     = (status & E1000_STATUS_LU) ? 1 : 0;
    info.full_duplex = (status & E1000_STATUS_FD) ? 1 : 0;

    /* Hiz algilama */
    uint32_t speed_bits = (status >> 6) & 0x03;
    switch (speed_bits) {
        case 0: info.speed = 10;   break;
        case 1: info.speed = 100;  break;
        case 2: info.speed = 1000; break;
        case 3: info.speed = 1000; break;
    }
}

/* ======== ANA INIT ======== */

int e1000_init(void) {
    k_memset(&info, 0, sizeof(info));

    /* PCI'da E1000 bul */
    pci_device_t* dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000);
    if (!dev) {
        /* Diger E1000 varyantlarini dene */
        dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000_2);
    }
    if (!dev) {
        dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000_3);
    }
    if (!dev) {
        dev = pci_find_class(PCI_CLASS_NETWORK, 0x00);
    }
    if (!dev) {
        return -1;  /* Ag karti bulunamadi */
    }

    /* PCI Bus Mastering etkinlestir */
    pci_enable_bus_mastering(dev);

    /* BAR0 = MMIO base address */
    mmio_addr = dev->bar[0] & ~0x0F;  /* Alt 4 bit mask */
    mmio_base = (volatile uint32_t*)(uint32_t)mmio_addr;

    if (mmio_addr == 0) {
        return -2;  /* BAR0 gecersiz */
    }

    /* 1. Karti resetle */
    uint32_t ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_RST;
    e1000_write(E1000_CTRL, ctrl);

    /* Reset'in tamamlanmasini bekle */
    for (volatile int i = 0; i < 100000; i++);

    /* 2. Interrupt'lari kapat (polling kullanacagiz) */
    e1000_write(E1000_IMC, 0xFFFFFFFF);
    e1000_read(E1000_ICR);  /* Bekleyen interrupt'lari temizle */

    /* 3. Link'i yukari kaldir */
    ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_SLU;    /* Set Link Up */
    ctrl |= E1000_CTRL_ASDE;   /* Auto-Speed Detection */
    ctrl &= ~E1000_CTRL_RST;   /* Reset bitini temizle */
    e1000_write(E1000_CTRL, ctrl);

    /* 4. Multicast tablosunu temizle */
    e1000_clear_multicast();

    /* 5. MAC adresini oku */
    e1000_read_mac();

    /* 6. RX ve TX baslatma */
    e1000_init_rx();
    e1000_init_tx();

    /* 7. Link durumunu kontrol et */
    /* Linkin gelmesi icin biraz bekle */
    for (volatile int i = 0; i < 500000; i++);
    e1000_check_link();

    info.initialized = 1;
    return 0;
}

/* ======== PAKET GONDER ======== */

int e1000_send(const void* data, uint16_t length) {
    if (!info.initialized) return -1;
    if (length == 0 || length > E1000_TX_BUF_SIZE) return -2;

    /* Mevcut descriptor bos mu? */
    if (!(tx_descs[tx_cur].status & E1000_TXD_STAT_DD)) {
        info.tx_errors++;
        return -3;  /* TX ring dolu */
    }

    /* Veriyi buffer'a kopyala */
    uint8_t* buf = tx_buffers[tx_cur];
    for (uint16_t i = 0; i < length; i++) {
        buf[i] = ((const uint8_t*)data)[i];
    }

    /* Descriptor'i doldur */
    tx_descs[tx_cur].length = length;
    tx_descs[tx_cur].cmd    = E1000_TXD_CMD_EOP |
                               E1000_TXD_CMD_IFCS |
                               E1000_TXD_CMD_RS;
    tx_descs[tx_cur].status = 0;

    /* Bir sonraki slot */
    uint32_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;

    /* Tail'i guncelle — kart gondericiye baslar */
    e1000_write(E1000_TDT, tx_cur);

    /* Gonderimin tamamlanmasini bekle (polling) */
    int timeout = 100000;
    while (timeout-- > 0) {
        if (tx_descs[old_cur].status & E1000_TXD_STAT_DD) {
            info.tx_count++;
            return 0;  /* Basarili */
        }
    }

    info.tx_errors++;
    return -4;  /* Timeout */
}

/* ======== PAKET AL ======== */

int e1000_receive(void* buffer, uint16_t max_length) {
    if (!info.initialized) return -1;

    /* Mevcut descriptor'da yeni paket var mi? */
    if (!(rx_descs[rx_cur].status & E1000_RXD_STAT_DD)) {
        return 0;  /* Paket yok */
    }

    /* Paket uzunlugu */
    uint16_t length = rx_descs[rx_cur].length;
    if (length > max_length) {
        length = max_length;
    }

    /* Veriyi kopyala */
    uint8_t* src = rx_buffers[rx_cur];
    uint8_t* dst = (uint8_t*)buffer;
    for (uint16_t i = 0; i < length; i++) {
        dst[i] = src[i];
    }

    /* Descriptor'i sifirla — tekrar kullanilabilir */
    rx_descs[rx_cur].status = 0;

    /* Tail'i guncelle */
    uint32_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    e1000_write(E1000_RDT, old_cur);

    info.rx_count++;
    return (int)length;
}

/* ======== YARDIMCI ======== */

e1000_info_t* e1000_get_info(void) {
    if (info.initialized) {
        e1000_check_link();
    }
    return &info;
}

int e1000_link_up(void) {
    if (!info.initialized) return 0;
    e1000_check_link();
    return info.link_up;
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        mac[i] = info.mac[i];
    }
}
