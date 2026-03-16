#include "net.h"
#include "e1000.h"
#include "string.h"

static net_config_t config;
static arp_entry_t arp_table[ARP_TABLE_SIZE];
static uint8_t rx_packet[2048];
static uint8_t tx_packet[2048];
static int ping_reply_received = 0;
static uint16_t ip_id_counter = 1;

/* UDP alma slotlari */
static udp_rx_buf_t udp_rx[UDP_RX_SLOTS];

/* ======== Yardimcilar ======== */

uint16_t net_ip_checksum(const void* data, int length) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    while (length > 1) { sum += *ptr++; length -= 2; }
    if (length == 1) sum += *(const uint8_t*)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static void mac_copy(uint8_t* d, const uint8_t* s) {
    for (int i=0;i<6;i++) d[i]=s[i];
}
static void ip_copy(uint8_t* d, const uint8_t* s) {
    for (int i=0;i<4;i++) d[i]=s[i];
}
static int ip_eq(const uint8_t* a, const uint8_t* b) {
    return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3];
}

/* ======== Init ======== */

void net_init(void) {
    k_memset(&config, 0, sizeof(config));
    k_memset(arp_table, 0, sizeof(arp_table));
    k_memset(udp_rx, 0, sizeof(udp_rx));
    ping_reply_received = 0;

    e1000_info_t* ni = e1000_get_info();
    if (!ni->initialized) return;

    /* VirtualBox NAT varsayilan IP */
    config.our_ip[0]=10; config.our_ip[1]=0;
    config.our_ip[2]=2;  config.our_ip[3]=15;
    config.gateway_ip[0]=10; config.gateway_ip[1]=0;
    config.gateway_ip[2]=2;  config.gateway_ip[3]=2;
    config.subnet_mask[0]=255; config.subnet_mask[1]=255;
    config.subnet_mask[2]=255; config.subnet_mask[3]=0;
    config.dns_ip[0]=10; config.dns_ip[1]=0;
    config.dns_ip[2]=2;  config.dns_ip[3]=3;
    config.configured = 1;
}

/* ======== ARP Tablosu ======== */

static void arp_table_add(const uint8_t ip[4], const uint8_t mac[6]) {
    for (int i=0;i<ARP_TABLE_SIZE;i++) {
        if (arp_table[i].valid && ip_eq(arp_table[i].ip,ip)) {
            mac_copy(arp_table[i].mac,mac);
            return;
        }
    }
    for (int i=0;i<ARP_TABLE_SIZE;i++) {
        if (!arp_table[i].valid) {
            ip_copy(arp_table[i].ip,ip);
            mac_copy(arp_table[i].mac,mac);
            arp_table[i].valid=1;
            return;
        }
    }
    ip_copy(arp_table[0].ip,ip);
    mac_copy(arp_table[0].mac,mac);
    arp_table[0].valid=1;
}

int net_arp_lookup(const uint8_t ip[4], uint8_t mac_out[6]) {
    for (int i=0;i<ARP_TABLE_SIZE;i++) {
        if (arp_table[i].valid && ip_eq(arp_table[i].ip,ip)) {
            mac_copy(mac_out,arp_table[i].mac);
            return 1;
        }
    }
    return 0;
}

/* ======== Ethernet Gonder ======== */

static int send_eth(const uint8_t dst[6], uint16_t type,
                    const void* payload, uint16_t plen) {
    if (plen + 14 > sizeof(tx_packet)) return -1;
    eth_header_t* eth = (eth_header_t*)tx_packet;
    mac_copy(eth->dst_mac, dst);
    uint8_t our_mac[6];
    e1000_get_mac(our_mac);
    mac_copy(eth->src_mac, our_mac);
    eth->ethertype = htons(type);
    uint8_t* d = tx_packet + 14;
    const uint8_t* s = (const uint8_t*)payload;
    for (uint16_t i=0;i<plen;i++) d[i]=s[i];
    uint16_t total = 14 + plen;
    if (total < 60) {
        for (uint16_t i=total;i<60;i++) tx_packet[i]=0;
        total=60;
    }
    return e1000_send(tx_packet, total);
}

/* ======== IP Gonder ======== */

static int resolve_mac(const uint8_t dst_ip[4], uint8_t mac_out[6]) {
    uint8_t next[4];
    int same = 1;
    for (int i = 0; i < 4; i++)
        if ((config.our_ip[i] & config.subnet_mask[i]) !=
            (dst_ip[i] & config.subnet_mask[i])) { same = 0; break; }

    if (same) ip_copy(next, dst_ip);
    else ip_copy(next, config.gateway_ip);

    if (net_arp_lookup(next, mac_out)) return 1;

    /* 3 deneme yap, her seferinde daha uzun bekle */
    for (int attempt = 0; attempt < 3; attempt++) {
        net_send_arp_request(next);

        /* Bekleme: her denemede artan */
        for (int w = 0; w < 20; w++) {
            for (volatile int d = 0; d < 500000; d++);
            net_poll();
            if (net_arp_lookup(next, mac_out)) return 1;
        }
    }

    return 0;
}

static int send_ip(const uint8_t dst_ip[4], uint8_t proto,
                   const void* payload, uint16_t plen) {
    if (!config.configured) return -1;
    uint8_t dst_mac[6];
    if (!resolve_mac(dst_ip, dst_mac)) {
        k_memset(dst_mac, 0xFF, 6);
    }
    uint8_t buf[2048];
    ip_header_t* ip = (ip_header_t*)buf;
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(20 + plen);
    ip->id = htons(ip_id_counter++);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = proto;
    ip->checksum = 0;
    ip_copy(ip->src_ip, config.our_ip);
    ip_copy(ip->dst_ip, dst_ip);
    ip->checksum = net_ip_checksum(ip, 20);
    uint8_t* p = buf + 20;
    const uint8_t* s = (const uint8_t*)payload;
    for (uint16_t i=0;i<plen;i++) p[i]=s[i];
    return send_eth(dst_mac, ETH_TYPE_IP, buf, 20+plen);
}

/* ======== ARP ======== */

int net_send_arp_request(const uint8_t target_ip[4]) {
    if (!config.configured) return -1;
    arp_packet_t arp;
    arp.hw_type=htons(1); arp.proto_type=htons(0x0800);
    arp.hw_len=6; arp.proto_len=4;
    arp.opcode=htons(ARP_REQUEST);
    uint8_t our_mac[6]; e1000_get_mac(our_mac);
    mac_copy(arp.sender_mac,our_mac);
    ip_copy(arp.sender_ip,config.our_ip);
    k_memset(arp.target_mac,0,6);
    ip_copy(arp.target_ip,target_ip);
    uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    config.arp_tx++;
    return send_eth(bcast, ETH_TYPE_ARP, &arp, sizeof(arp));
}

static int send_arp_reply(const uint8_t dst_mac[6], const uint8_t dst_ip[4]) {
    arp_packet_t arp;
    arp.hw_type=htons(1); arp.proto_type=htons(0x0800);
    arp.hw_len=6; arp.proto_len=4;
    arp.opcode=htons(ARP_REPLY);
    uint8_t our_mac[6]; e1000_get_mac(our_mac);
    mac_copy(arp.sender_mac,our_mac);
    ip_copy(arp.sender_ip,config.our_ip);
    mac_copy(arp.target_mac,dst_mac);
    ip_copy(arp.target_ip,dst_ip);
    config.arp_tx++;
    return send_eth(dst_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/* ======== PING ======== */

int net_send_ping(const uint8_t dst_ip[4], uint16_t seq) {
    uint8_t buf[sizeof(icmp_header_t)+32];
    icmp_header_t* icmp = (icmp_header_t*)buf;
    icmp->type=ICMP_ECHO_REQUEST; icmp->code=0;
    icmp->checksum=0; icmp->id=htons(0x1234); icmp->seq=htons(seq);
    for (int i=0;i<32;i++) buf[sizeof(icmp_header_t)+i]=(uint8_t)('A'+(i%26));
    icmp->checksum = net_ip_checksum(buf, sizeof(buf));
    ping_reply_received=0;
    config.icmp_tx++;
    return send_ip(dst_ip, IP_PROTO_ICMP, buf, sizeof(buf));
}

int net_get_ping_reply(void) { return ping_reply_received; }

/* ======== UDP Gonder ======== */

int net_send_udp(const uint8_t dst_ip[4], uint16_t src_port,
                 uint16_t dst_port, const void* data, uint16_t len) {
    if (!config.configured) return -1;
    uint16_t udp_len = 8 + len;
    uint8_t buf[2048];
    udp_header_t* udp = (udp_header_t*)buf;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons(udp_len);
    udp->checksum = 0;  /* Opsiyonel IPv4 icin */
    const uint8_t* s = (const uint8_t*)data;
    for (uint16_t i=0;i<len;i++) buf[8+i]=s[i];
    config.udp_tx++;
    return send_ip(dst_ip, IP_PROTO_UDP, buf, udp_len);
}

/* ======== UDP Al ======== */

int net_recv_udp(uint16_t port, void* buffer, uint16_t max_len) {
    for (int i=0;i<UDP_RX_SLOTS;i++) {
        if (udp_rx[i].ready && udp_rx[i].port == port) {
            uint16_t len = udp_rx[i].length;
            if (len > max_len) len = max_len;
            uint8_t* d = (uint8_t*)buffer;
            for (uint16_t j=0;j<len;j++) d[j]=udp_rx[i].data[j];
            udp_rx[i].ready = 0;
            return (int)len;
        }
    }
    return 0;
}

/* ======== Paket Isleme ======== */

static void handle_arp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(arp_packet_t)) return;
    const arp_packet_t* arp = (const arp_packet_t*)data;
    arp_table_add(arp->sender_ip, arp->sender_mac);
    config.arp_rx++;
    if (ntohs(arp->opcode)==ARP_REQUEST && ip_eq(arp->target_ip,config.our_ip))
        send_arp_reply(arp->sender_mac, arp->sender_ip);
}

static void handle_udp(const uint8_t* data, uint16_t len,
                       const uint8_t src_ip[4]) {
    if (len < 8) return;
    const udp_header_t* udp = (const udp_header_t*)data;
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t udp_len  = ntohs(udp->length);
    uint16_t payload_len = udp_len - 8;
    if (payload_len > len - 8) payload_len = len - 8;
    config.udp_rx++;

    /* Bos slot bul ve kaydet */
    for (int i=0;i<UDP_RX_SLOTS;i++) {
        if (!udp_rx[i].ready) {
            udp_rx[i].port = dst_port;
            udp_rx[i].length = payload_len > UDP_RX_MAX ? UDP_RX_MAX : payload_len;
            ip_copy(udp_rx[i].src_ip, src_ip);
            const uint8_t* p = data + 8;
            for (uint16_t j=0;j<udp_rx[i].length;j++)
                udp_rx[i].data[j]=p[j];
            udp_rx[i].ready = 1;
            break;
        }
    }
}

static void handle_icmp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;
    const icmp_header_t* icmp = (const icmp_header_t*)data;
    if (icmp->type == ICMP_ECHO_REPLY) {
        ping_reply_received = 1;
        config.icmp_rx++;
    } else if (icmp->type == ICMP_ECHO_REQUEST) {
        config.icmp_rx++;
    }
}

static void handle_ip(const uint8_t* data, uint16_t len) {
    if (len < 20) return;
    const ip_header_t* ip = (const ip_header_t*)data;
    uint16_t hdr_len = (ip->ver_ihl & 0x0F) * 4;
    uint16_t total = ntohs(ip->total_length);
    if (total > len) total = len;
    const uint8_t* payload = data + hdr_len;
    uint16_t plen = total - hdr_len;

    switch (ip->protocol) {
        case IP_PROTO_ICMP: handle_icmp(payload, plen); break;
        case IP_PROTO_UDP:  handle_udp(payload, plen, ip->src_ip); break;
        case IP_PROTO_TCP:  break; /* Adim 4 */
    }
}

void net_poll(void) {
    if (!config.configured) return;
    for (int a=0;a<8;a++) {
        int len = e1000_receive(rx_packet, sizeof(rx_packet));
        if (len <= 0) break;
        if (len < 14) continue;
        const eth_header_t* eth = (const eth_header_t*)rx_packet;
        uint16_t type = ntohs(eth->ethertype);
        const uint8_t* payload = rx_packet + 14;
        uint16_t plen = (uint16_t)(len - 14);
        if (type == ETH_TYPE_ARP) handle_arp(payload, plen);
        else if (type == ETH_TYPE_IP) handle_ip(payload, plen);
    }
}

net_config_t* net_get_config(void) { return &config; }
