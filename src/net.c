#include "net.h"
#include "e1000.h"
#include "string.h"

static net_config_t config;
static arp_entry_t arp_table[ARP_TABLE_SIZE];
static uint8_t rx_packet[2048];
static uint8_t tx_packet[2048];
static int ping_reply_received = 0;

/* ======== Checksum ======== */

static uint16_t ip_checksum(const void* data, int length) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    if (length == 1) {
        sum += *(const uint8_t*)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* ======== MAC karsilastir ======== */

static int mac_eq(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 6; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static int ip_eq(const uint8_t* a, const uint8_t* b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

static void mac_copy(uint8_t* dst, const uint8_t* src) {
    for (int i = 0; i < 6; i++) dst[i] = src[i];
}

static void ip_copy(uint8_t* dst, const uint8_t* src) {
    for (int i = 0; i < 4; i++) dst[i] = src[i];
}

/* ======== Init ======== */

void net_init(void) {
    k_memset(&config, 0, sizeof(config));
    k_memset(arp_table, 0, sizeof(arp_table));
    ping_reply_received = 0;

    e1000_info_t* ni = e1000_get_info();
    if (!ni->initialized) return;

    /* Varsayilan IP yapilandirmasi (VirtualBox NAT icin) */
    config.our_ip[0] = 10;
    config.our_ip[1] = 0;
    config.our_ip[2] = 2;
    config.our_ip[3] = 15;

    config.gateway_ip[0] = 10;
    config.gateway_ip[1] = 0;
    config.gateway_ip[2] = 2;
    config.gateway_ip[3] = 2;

    config.subnet_mask[0] = 255;
    config.subnet_mask[1] = 255;
    config.subnet_mask[2] = 255;
    config.subnet_mask[3] = 0;

    config.dns_ip[0] = 10;
    config.dns_ip[1] = 0;
    config.dns_ip[2] = 2;
    config.dns_ip[3] = 3;

    config.configured = 1;
}

/* ======== ARP Tablosu ======== */

static void arp_table_add(const uint8_t ip[4], const uint8_t mac[6]) {
    /* Zaten var mi? */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && ip_eq(arp_table[i].ip, ip)) {
            mac_copy(arp_table[i].mac, mac);
            return;
        }
    }
    /* Bos slot bul */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            ip_copy(arp_table[i].ip, ip);
            mac_copy(arp_table[i].mac, mac);
            arp_table[i].valid = 1;
            return;
        }
    }
    /* Dolu — ilk slota yaz */
    ip_copy(arp_table[0].ip, ip);
    mac_copy(arp_table[0].mac, mac);
    arp_table[0].valid = 1;
}

int net_arp_lookup(const uint8_t ip[4], uint8_t mac_out[6]) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && ip_eq(arp_table[i].ip, ip)) {
            mac_copy(mac_out, arp_table[i].mac);
            return 1;
        }
    }
    return 0;
}

/* ======== Ethernet Frame Gonder ======== */

static int send_eth_frame(const uint8_t dst_mac[6], uint16_t ethertype,
                           const void* payload, uint16_t payload_len) {
    if (payload_len + sizeof(eth_header_t) > sizeof(tx_packet))
        return -1;

    eth_header_t* eth = (eth_header_t*)tx_packet;
    mac_copy(eth->dst_mac, dst_mac);

    uint8_t our_mac[6];
    e1000_get_mac(our_mac);
    mac_copy(eth->src_mac, our_mac);

    eth->ethertype = htons(ethertype);

    /* Payload kopyala */
    uint8_t* data = tx_packet + sizeof(eth_header_t);
    const uint8_t* src = (const uint8_t*)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        data[i] = src[i];

    uint16_t total = sizeof(eth_header_t) + payload_len;

    /* Minimum Ethernet frame: 60 byte (CRC haric) */
    if (total < 60) {
        for (uint16_t i = total; i < 60; i++)
            tx_packet[i] = 0;
        total = 60;
    }

    return e1000_send(tx_packet, total);
}

/* ======== ARP Request Gonder ======== */

int net_send_arp_request(const uint8_t target_ip[4]) {
    if (!config.configured) return -1;

    arp_packet_t arp;
    arp.hw_type    = htons(1);
    arp.proto_type = htons(0x0800);
    arp.hw_len     = 6;
    arp.proto_len  = 4;
    arp.opcode     = htons(ARP_REQUEST);

    uint8_t our_mac[6];
    e1000_get_mac(our_mac);
    mac_copy(arp.sender_mac, our_mac);
    ip_copy(arp.sender_ip, config.our_ip);

    /* Hedef MAC bilinmiyor */
    k_memset(arp.target_mac, 0, 6);
    ip_copy(arp.target_ip, target_ip);

    /* Broadcast MAC */
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    config.arp_tx++;
    return send_eth_frame(broadcast, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/* ======== ARP Reply Gonder ======== */

static int send_arp_reply(const uint8_t dst_mac[6], const uint8_t dst_ip[4]) {
    arp_packet_t arp;
    arp.hw_type    = htons(1);
    arp.proto_type = htons(0x0800);
    arp.hw_len     = 6;
    arp.proto_len  = 4;
    arp.opcode     = htons(ARP_REPLY);

    uint8_t our_mac[6];
    e1000_get_mac(our_mac);
    mac_copy(arp.sender_mac, our_mac);
    ip_copy(arp.sender_ip, config.our_ip);
    mac_copy(arp.target_mac, dst_mac);
    ip_copy(arp.target_ip, dst_ip);

    config.arp_tx++;
    return send_eth_frame(dst_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/* ======== IP Paketi Gonder ======== */

static int send_ip_packet(const uint8_t dst_ip[4], uint8_t protocol,
                           const void* payload, uint16_t payload_len) {
    if (!config.configured) return -1;

    /* Hedef MAC bul — ayni subnette mi? */
    uint8_t next_hop[4];
    int same_subnet = 1;
    for (int i = 0; i < 4; i++) {
        if ((config.our_ip[i] & config.subnet_mask[i]) !=
            (dst_ip[i] & config.subnet_mask[i])) {
            same_subnet = 0;
            break;
        }
    }

    if (same_subnet) {
        ip_copy(next_hop, dst_ip);
    } else {
        ip_copy(next_hop, config.gateway_ip);
    }

    uint8_t dst_mac[6];
    if (!net_arp_lookup(next_hop, dst_mac)) {
        /* ARP tablosunda yok — request gonder ve varsayilan broadcast kullan */
        net_send_arp_request(next_hop);
        /* Biraz bekle */
        for (volatile int i = 0; i < 500000; i++);
        /* Gelen paketleri isle */
        net_poll();
        /* Tekrar dene */
        if (!net_arp_lookup(next_hop, dst_mac)) {
            /* Hala yok — broadcast */
            k_memset(dst_mac, 0xFF, 6);
        }
    }

    /* IP header */
    uint8_t ip_buf[2048];
    ip_header_t* ip = (ip_header_t*)ip_buf;
    ip->ver_ihl      = 0x45;  /* IPv4, 5 dwords (20 bytes) */
    ip->tos           = 0;
    ip->total_length  = htons(20 + payload_len);
    ip->id            = htons(1);
    ip->flags_frag    = 0;
    ip->ttl           = 64;
    ip->protocol      = protocol;
    ip->checksum      = 0;
    ip_copy(ip->src_ip, config.our_ip);
    ip_copy(ip->dst_ip, dst_ip);

    /* Checksum hesapla */
    ip->checksum = ip_checksum(ip, 20);

    /* Payload ekle */
    uint8_t* p = ip_buf + 20;
    const uint8_t* src = (const uint8_t*)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        p[i] = src[i];

    return send_eth_frame(dst_mac, ETH_TYPE_IP, ip_buf, 20 + payload_len);
}

/* ======== PING Gonder ======== */

int net_send_ping(const uint8_t dst_ip[4], uint16_t seq) {
    icmp_header_t icmp;
    icmp.type     = ICMP_ECHO_REQUEST;
    icmp.code     = 0;
    icmp.checksum = 0;
    icmp.id       = htons(0x1234);
    icmp.seq      = htons(seq);

    /* 32 byte veri ekle */
    uint8_t icmp_buf[sizeof(icmp_header_t) + 32];
    uint8_t* ptr = icmp_buf;
    for (int i = 0; i < (int)sizeof(icmp_header_t); i++)
        ptr[i] = ((uint8_t*)&icmp)[i];
    for (int i = 0; i < 32; i++)
        ptr[sizeof(icmp_header_t) + i] = (uint8_t)('A' + (i % 26));

    /* Checksum */
    uint16_t cksum = ip_checksum(icmp_buf, sizeof(icmp_buf));
    icmp_buf[2] = (uint8_t)(cksum & 0xFF);
    icmp_buf[3] = (uint8_t)(cksum >> 8);

    ping_reply_received = 0;
    config.icmp_tx++;
    return send_ip_packet(dst_ip, IP_PROTO_ICMP, icmp_buf, sizeof(icmp_buf));
}

int net_get_ping_reply(void) {
    return ping_reply_received;
}

/* ======== Gelen Paketleri Isle ======== */

static void handle_arp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(arp_packet_t)) return;

    const arp_packet_t* arp = (const arp_packet_t*)data;

    /* ARP tablosuna ekle */
    arp_table_add(arp->sender_ip, arp->sender_mac);
    config.arp_rx++;

    /* Bize gelen ARP request mi? */
    if (ntohs(arp->opcode) == ARP_REQUEST) {
        if (ip_eq(arp->target_ip, config.our_ip)) {
            send_arp_reply(arp->sender_mac, arp->sender_ip);
        }
    }
}

static void handle_icmp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;

    const icmp_header_t* icmp = (const icmp_header_t*)data;

    if (icmp->type == ICMP_ECHO_REPLY) {
        ping_reply_received = 1;
        config.icmp_rx++;
    }
    else if (icmp->type == ICMP_ECHO_REQUEST) {
        /* Ping'e cevap ver */
        /* IP header'dan kaynak IP'yi al */
        /* Bu basitlestirmede cevaplayamayiz — sonra ekleriz */
        config.icmp_rx++;
    }
}

static void handle_ip(const uint8_t* data, uint16_t len) {
    if (len < sizeof(ip_header_t)) return;

    const ip_header_t* ip = (const ip_header_t*)data;
    uint16_t ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
    uint16_t total_len = ntohs(ip->total_length);

    if (total_len > len) return;

    const uint8_t* payload = data + ip_hdr_len;
    uint16_t payload_len = total_len - ip_hdr_len;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            handle_icmp(payload, payload_len);
            break;
        case IP_PROTO_UDP:
            /* Adim 3'te eklenecek */
            break;
        case IP_PROTO_TCP:
            /* Adim 4'te eklenecek */
            break;
    }
}

void net_poll(void) {
    if (!config.configured) return;

    /* Birden fazla paket olabilir */
    for (int attempt = 0; attempt < 8; attempt++) {
        int len = e1000_receive(rx_packet, sizeof(rx_packet));
        if (len <= 0) break;
        if (len < (int)sizeof(eth_header_t)) continue;

        const eth_header_t* eth = (const eth_header_t*)rx_packet;
        const uint8_t* payload = rx_packet + sizeof(eth_header_t);
        uint16_t payload_len = (uint16_t)(len - sizeof(eth_header_t));
        uint16_t ethertype = ntohs(eth->ethertype);

        switch (ethertype) {
            case ETH_TYPE_ARP:
                handle_arp(payload, payload_len);
                break;
            case ETH_TYPE_IP:
                handle_ip(payload, payload_len);
                break;
        }
    }
}

net_config_t* net_get_config(void) {
    return &config;
}
