#ifndef NET_H
#define NET_H

#include <stdint.h>

/* Ethernet */
typedef struct {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IP   0x0800

/* ARP */
typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
} __attribute__((packed)) arp_packet_t;

#define ARP_REQUEST 1
#define ARP_REPLY   2

/* IPv4 */
typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
} __attribute__((packed)) ip_header_t;

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

/* ICMP */
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

/* UDP */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

/* ARP tablosu */
#define ARP_TABLE_SIZE 16
typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    int     valid;
} arp_entry_t;

/* UDP alma bufferi */
#define UDP_RX_MAX 512
typedef struct {
    uint8_t  data[UDP_RX_MAX];
    uint16_t length;
    uint16_t port;
    uint8_t  src_ip[4];
    int      ready;
} udp_rx_buf_t;

#define UDP_RX_SLOTS 4

/* Ag yapilandirmasi */
typedef struct {
    uint8_t  our_ip[4];
    uint8_t  gateway_ip[4];
    uint8_t  subnet_mask[4];
    uint8_t  dns_ip[4];
    int      configured;
    uint32_t icmp_tx;
    uint32_t icmp_rx;
    uint32_t arp_tx;
    uint32_t arp_rx;
    uint32_t udp_tx;
    uint32_t udp_rx;
} net_config_t;

/* Fonksiyonlar */
void           net_init(void);
void           net_poll(void);
int            net_send_arp_request(const uint8_t target_ip[4]);
int            net_send_ping(const uint8_t dst_ip[4], uint16_t seq);
int            net_arp_lookup(const uint8_t ip[4], uint8_t mac_out[6]);
int            net_get_ping_reply(void);
net_config_t*  net_get_config(void);
int            net_send_udp(const uint8_t dst_ip[4], uint16_t src_port,
                            uint16_t dst_port, const void* data, uint16_t len);
int            net_recv_udp(uint16_t port, void* buffer, uint16_t max_len);
uint16_t       net_ip_checksum(const void* data, int length);

/* Byte order */
static inline uint16_t htons(uint16_t v) { return (v>>8)|(v<<8); }
static inline uint16_t ntohs(uint16_t v) { return (v>>8)|(v<<8); }
static inline uint32_t htonl(uint32_t v) {
    return ((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

#endif
