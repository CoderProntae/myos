#ifndef E1000_H
#define E1000_H

#include <stdint.h>

/* E1000 Register Offsetleri */
#define E1000_CTRL     0x0000
#define E1000_STATUS   0x0008
#define E1000_EERD     0x0014
#define E1000_ICR      0x00C0
#define E1000_IMS      0x00D0
#define E1000_IMC      0x00D8
#define E1000_RCTL     0x0100
#define E1000_TCTL     0x0400
#define E1000_TIPG     0x0410
#define E1000_RDBAL    0x2800
#define E1000_RDBAH    0x2804
#define E1000_RDLEN    0x2808
#define E1000_RDH      0x2810
#define E1000_RDT      0x2818
#define E1000_TDBAL    0x3800
#define E1000_TDBAH    0x3804
#define E1000_TDLEN    0x3808
#define E1000_TDH      0x3810
#define E1000_TDT      0x3818
#define E1000_RAL      0x5400
#define E1000_RAH      0x5404
#define E1000_MTA      0x5200

/* CTRL register bitleri */
#define E1000_CTRL_SLU     (1 << 6)   /* Set Link Up */
#define E1000_CTRL_RST     (1 << 26)  /* Reset */
#define E1000_CTRL_ASDE    (1 << 5)   /* Auto-Speed Detection */

/* RCTL register bitleri */
#define E1000_RCTL_EN      (1 << 1)   /* Receiver Enable */
#define E1000_RCTL_SBP     (1 << 2)   /* Store Bad Packets */
#define E1000_RCTL_UPE     (1 << 3)   /* Unicast Promisc */
#define E1000_RCTL_MPE     (1 << 4)   /* Multicast Promisc */
#define E1000_RCTL_BAM     (1 << 15)  /* Broadcast Accept */
#define E1000_RCTL_BSIZE_2048  (0 << 16)
#define E1000_RCTL_SECRC   (1 << 26)  /* Strip CRC */

/* TCTL register bitleri */
#define E1000_TCTL_EN      (1 << 1)   /* Transmitter Enable */
#define E1000_TCTL_PSP     (1 << 3)   /* Pad Short Packets */

/* STATUS register bitleri */
#define E1000_STATUS_LU    (1 << 1)   /* Link Up */
#define E1000_STATUS_FD    (1 << 0)   /* Full Duplex */

/* TX Descriptor komutu */
#define E1000_TXD_CMD_RS   (1 << 3)   /* Report Status */
#define E1000_TXD_CMD_EOP  (1 << 0)   /* End Of Packet */
#define E1000_TXD_CMD_IFCS (1 << 1)   /* Insert FCS/CRC */

/* RX Descriptor durum */
#define E1000_RXD_STAT_DD  (1 << 0)   /* Descriptor Done */
#define E1000_RXD_STAT_EOP (1 << 1)   /* End Of Packet */

/* TX Descriptor durum */
#define E1000_TXD_STAT_DD  (1 << 0)   /* Descriptor Done */

/* Ring boyutlari */
#define E1000_NUM_RX_DESC  32
#define E1000_NUM_TX_DESC  32
#define E1000_RX_BUF_SIZE  2048
#define E1000_TX_BUF_SIZE  2048

/* RX Descriptor */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

/* TX Descriptor */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

/* Ag karti bilgisi */
typedef struct {
    uint8_t  mac[6];
    int      link_up;
    int      speed;       /* Mbps */
    int      full_duplex;
    int      initialized;
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t tx_errors;
    uint32_t rx_errors;
} e1000_info_t;

/* Fonksiyonlar */
int            e1000_init(void);
int            e1000_send(const void* data, uint16_t length);
int            e1000_receive(void* buffer, uint16_t max_length);
e1000_info_t*  e1000_get_info(void);
int            e1000_link_up(void);
void           e1000_get_mac(uint8_t mac[6]);

#endif
