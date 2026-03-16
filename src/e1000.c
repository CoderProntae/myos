#include "e1000.h"
#include "pci.h"
#include "io.h"
#include "string.h"

/* MMIO */
static uint32_t mmio_addr = 0;
static int use_io = 0;
static uint16_t io_base = 0;

/* Descriptor ring — MUTLAKA 128-byte aligned */
static e1000_rx_desc_t rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(128)));
static e1000_tx_desc_t tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(128)));

/* Paket bufferlari — 16-byte aligned */
static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_TX_BUF_SIZE] __attribute__((aligned(16)));

static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;
static e1000_info_t info;

/* 32-bit port I/O */
static void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}
static uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ======== Register Erisim ======== */

static void e1000_write(uint32_t reg, uint32_t value) {
    if (use_io) {
        outl(io_base, reg);
        outl(io_base + 4, value);
    } else {
        volatile uint32_t* ptr = (volatile uint32_t*)(mmio_addr + reg);
        *ptr = value;
    }
}

static uint32_t e1000_read(uint32_t reg) {
    if (use_io) {
        outl(io_base, reg);
        return inl(io_base + 4);
    } else {
        volatile uint32_t* ptr = (volatile uint32_t*)(mmio_addr + reg);
        return *ptr;
    }
}

/* ======== EEPROM ======== */

static int eeprom_exists = 0;

static void e1000_detect_eeprom(void) {
    e1000_write(E1000_EERD, 1);
    for (int i = 0; i < 100000; i++) {
        uint32_t val = e1000_read(E1000_EERD);
        if (val & 0x10) { eeprom_exists = 1; return; }
    }
    eeprom_exists = 0;
}

static uint16_t e1000_eeprom_read(uint8_t addr) {
    uint32_t val;
    if (eeprom_exists) {
        e1000_write(E1000_EERD, ((uint32_t)addr << 8) | 1);
        for (int i = 0; i < 100000; i++) {
            val = e1000_read(E1000_EERD);
            if (val & (1 << 4))
                return (uint16_t)(val >> 16);
        }
    } else {
        /* EERD bitmedi — diger yontem */
        e1000_write(E1000_EERD, ((uint32_t)addr << 2) | 1);
        for (int i = 0; i < 100000; i++) {
            val = e1000_read(E1000_EERD);
            if (val & (1 << 1))
                return (uint16_t)(val >> 16);
        }
    }
    return 0;
}

/* ======== MAC Oku ======== */

static void e1000_read_mac(void) {
    /* Oncelikle EEPROM'dan dene */
    e1000_detect_eeprom();

    uint16_t mac01 = e1000_eeprom_read(0);
    uint16_t mac23 = e1000_eeprom_read(1);
    uint16_t mac45 = e1000_eeprom_read(2);

    if (mac01 == 0 && mac23 == 0 && mac45 == 0) {
        /* EEPROM okuma basarisiz — RAL/RAH'dan oku */
        uint32_t ral = e1000_read(E1000_RAL);
        uint32_t rah = e1000_read(E1000_RAH);
        info.mac[0] = (uint8_t)(ral);
        info.mac[1] = (uint8_t)(ral >> 8);
        info.mac[2] = (uint8_t)(ral >> 16);
        info.mac[3] = (uint8_t)(ral >> 24);
        info.mac[4] = (uint8_t)(rah);
        info.mac[5] = (uint8_t)(rah >> 8);
    } else {
        info.mac[0] = (uint8_t)(mac01);
        info.mac[1] = (uint8_t)(mac01 >> 8);
        info.mac[2] = (uint8_t)(mac23);
        info.mac[3] = (uint8_t)(mac23 >> 8);
        info.mac[4] = (uint8_t)(mac45);
        info.mac[5] = (uint8_t)(mac45 >> 8);
    }

    /* MAC'i donanim filtresine yaz */
    uint32_t ral_val = (uint32_t)info.mac[0] |
                       ((uint32_t)info.mac[1] << 8) |
                       ((uint32_t)info.mac[2] << 16) |
                       ((uint32_t)info.mac[3] << 24);
    uint32_t rah_val = (uint32_t)info.mac[4] |
                       ((uint32_t)info.mac[5] << 8) |
                       (1u << 31);
    e1000_write(E1000_RAL, ral_val);
    e1000_write(E1000_RAH, rah_val);
}

/* ======== RX Init ======== */

static void e1000_init_rx(void) {
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr   = (uint64_t)(uint32_t)rx_buffers[i];
        rx_descs[i].status = 0;
    }

    uint32_t rd_addr = (uint32_t)rx_descs;
    e1000_write(E1000_RDBAL, rd_addr);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);
    rx_cur = 0;

    e1000_write(E1000_RCTL,
        E1000_RCTL_EN |
        E1000_RCTL_BAM |
        E1000_RCTL_BSIZE_2048 |
        E1000_RCTL_SECRC |
        E1000_RCTL_UPE |
        E1000_RCTL_MPE);
}

/* ======== TX Init ======== */

static void e1000_init_tx(void) {
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr   = (uint64_t)(uint32_t)tx_buffers[i];
        tx_descs[i].status = E1000_TXD_STAT_DD;
        tx_descs[i].cmd    = 0;
    }

    uint32_t td_addr = (uint32_t)tx_descs;
    e1000_write(E1000_TDBAL, td_addr);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    tx_cur = 0;

    e1000_write(E1000_TIPG, 10 | (10 << 10) | (10 << 20));

    e1000_write(E1000_TCTL,
        E1000_TCTL_EN |
        E1000_TCTL_PSP |
        (15 << 4) |
        (64 << 12));
}

/* ======== Link ======== */

static void e1000_check_link(void) {
    uint32_t status = e1000_read(E1000_STATUS);
    info.link_up     = (status & E1000_STATUS_LU) ? 1 : 0;
    info.full_duplex = (status & E1000_STATUS_FD) ? 1 : 0;
    uint32_t spd = (status >> 6) & 3;
    if (spd == 0) info.speed = 10;
    else if (spd == 1) info.speed = 100;
    else info.speed = 1000;
}

/* ======== Init ======== */

int e1000_init(void) {
    k_memset(&info, 0, sizeof(info));

    pci_device_t* dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000);
    if (!dev) dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000_2);
    if (!dev) dev = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000_3);
    if (!dev) dev = pci_find_class(PCI_CLASS_NETWORK, 0x00);
    if (!dev) return -1;

    pci_enable_bus_mastering(dev);

    /* BAR0 kontrol — MMIO mi I/O mi? */
    if (dev->bar[0] & 1) {
        /* I/O space */
        io_base = (uint16_t)(dev->bar[0] & ~3);
        use_io = 1;
    } else {
        /* Memory mapped */
        mmio_addr = dev->bar[0] & ~0xF;
        use_io = 0;
    }

    /* BAR1 de I/O olabilir */
    if (!use_io && dev->bar[1] & 1) {
        io_base = (uint16_t)(dev->bar[1] & ~3);
    }

    if (mmio_addr == 0 && !use_io) return -2;

    /* Test: STATUS register okunabiliyor mu? */
    uint32_t test = e1000_read(E1000_STATUS);
    if (test == 0xFFFFFFFF) {
        /* MMIO calismadi — I/O dene */
        if (io_base) {
            use_io = 1;
            test = e1000_read(E1000_STATUS);
            if (test == 0xFFFFFFFF) return -3;
        } else {
            return -3;
        }
    }

    /* Reset */
    e1000_write(E1000_IMC, 0xFFFFFFFF);
    e1000_read(E1000_ICR);

    uint32_t ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_RST;
    e1000_write(E1000_CTRL, ctrl);

    for (volatile int i = 0; i < 1000000; i++);

    /* Interrupt kapat */
    e1000_write(E1000_IMC, 0xFFFFFFFF);
    e1000_read(E1000_ICR);

    /* Link UP */
    ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE;
    ctrl &= ~E1000_CTRL_RST;
    e1000_write(E1000_CTRL, ctrl);

    for (volatile int i = 0; i < 500000; i++);

    /* Multicast temizle */
    for (int i = 0; i < 128; i++)
        e1000_write(E1000_MTA + i * 4, 0);

    /* MAC oku */
    e1000_read_mac();

    /* RX ve TX */
    e1000_init_rx();
    e1000_init_tx();

    /* Link bekle */
    for (int w = 0; w < 10; w++) {
        for (volatile int d = 0; d < 500000; d++);
        e1000_check_link();
        if (info.link_up) break;
    }

    info.initialized = 1;
    return 0;
}

/* ======== Send ======== */

int e1000_send(const void* data, uint16_t length) {
    if (!info.initialized) return -1;
    if (length == 0 || length > E1000_TX_BUF_SIZE) return -2;

    /* Descriptor bos mu? */
    volatile e1000_tx_desc_t* desc = &tx_descs[tx_cur];

    /* Onceki gonderim tamamlanmamissa bekle */
    int timeout = 100000;
    while (!(desc->status & E1000_TXD_STAT_DD) && timeout > 0) {
        timeout--;
    }

    /* Buffer'a kopyala */
    uint8_t* buf = tx_buffers[tx_cur];
    for (uint16_t i = 0; i < length; i++)
        buf[i] = ((const uint8_t*)data)[i];

    /* Descriptor'i doldur */
    desc->addr   = (uint64_t)(uint32_t)buf;
    desc->length = length;
    desc->cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0;

    /* Sonraki slot */
    uint32_t old = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;

    /* TAIL yazarak gonderimi baslat */
    e1000_write(E1000_TDT, tx_cur);

    /* Tamamlanmasini bekle */
    timeout = 1000000;
    while (timeout > 0) {
        if (tx_descs[old].status & E1000_TXD_STAT_DD) {
            info.tx_count++;
            return 0;
        }
        timeout--;
    }

    info.tx_errors++;
    return -4;
}

/* ======== Receive ======== */

int e1000_receive(void* buffer, uint16_t max_length) {
    if (!info.initialized) return -1;

    volatile e1000_rx_desc_t* desc = &rx_descs[rx_cur];

    if (!(desc->status & E1000_RXD_STAT_DD))
        return 0;

    uint16_t length = desc->length;
    if (length > max_length) length = max_length;

    uint8_t* src = rx_buffers[rx_cur];
    uint8_t* dst = (uint8_t*)buffer;
    for (uint16_t i = 0; i < length; i++)
        dst[i] = src[i];

    /* Descriptor sifirla */
    desc->status = 0;
    uint32_t old = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    e1000_write(E1000_RDT, old);

    info.rx_count++;
    return (int)length;
}

/* ======== Info ======== */

e1000_info_t* e1000_get_info(void) {
    if (info.initialized) e1000_check_link();
    return &info;
}

int e1000_link_up(void) {
    if (!info.initialized) return 0;
    e1000_check_link();
    return info.link_up;
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = info.mac[i];
}
