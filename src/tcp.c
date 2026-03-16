#include "tcp.h"
#include "net.h"
#include "e1000.h"
#include "string.h"
#include "io.h"

static tcp_socket_t sockets[TCP_MAX_SOCKETS];
static uint16_t next_port = 49152;

static void ipcpy(uint8_t* d, const uint8_t* s) {
    d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=s[3];
}
static int ipeq(const uint8_t* a, const uint8_t* b) {
    return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3];
}

void tcp_init(void) {
    k_memset(sockets, 0, sizeof(sockets));
    next_port = 49152;
}

/* ======== Raw frame gonder ======== */

static int tcp_send_frame(tcp_socket_t* sock, uint8_t flags,
                           const void* payload, uint16_t payload_len) {
    net_config_t* nc = net_get_config();
    if (!nc->configured) return -1;

    uint8_t gw_mac[6];
    if (!net_arp_lookup(nc->gateway_ip, gw_mac)) return -10;

    uint8_t our_mac[6];
    e1000_get_mac(our_mac);

    uint16_t tcp_hlen = 20;
    uint16_t ip_total = 20 + tcp_hlen + payload_len;

    uint8_t pkt[1600];
    k_memset(pkt, 0, sizeof(pkt));
    uint16_t p = 0;

    /* ---- Ethernet (14 byte) ---- */
    pkt[p++]=gw_mac[0]; pkt[p++]=gw_mac[1]; pkt[p++]=gw_mac[2];
    pkt[p++]=gw_mac[3]; pkt[p++]=gw_mac[4]; pkt[p++]=gw_mac[5];
    pkt[p++]=our_mac[0]; pkt[p++]=our_mac[1]; pkt[p++]=our_mac[2];
    pkt[p++]=our_mac[3]; pkt[p++]=our_mac[4]; pkt[p++]=our_mac[5];
    pkt[p++]=0x08; pkt[p++]=0x00;

    /* ---- IP (20 byte) ---- */
    uint16_t ip_off = p;
    pkt[p++] = 0x45;                              /* ver=4, ihl=5 */
    pkt[p++] = 0x00;                              /* tos */
    pkt[p++] = (uint8_t)(ip_total >> 8);           /* total length */
    pkt[p++] = (uint8_t)(ip_total);
    pkt[p++] = 0x00; pkt[p++] = 0x00;             /* id */
    pkt[p++] = 0x40; pkt[p++] = 0x00;             /* flags=DF */
    pkt[p++] = 64;                                 /* ttl */
    pkt[p++] = 6;                                  /* protocol=TCP */
    pkt[p++] = 0x00; pkt[p++] = 0x00;             /* checksum (sonra) */
    pkt[p++]=nc->our_ip[0]; pkt[p++]=nc->our_ip[1];
    pkt[p++]=nc->our_ip[2]; pkt[p++]=nc->our_ip[3];
    pkt[p++]=sock->remote_ip[0]; pkt[p++]=sock->remote_ip[1];
    pkt[p++]=sock->remote_ip[2]; pkt[p++]=sock->remote_ip[3];

    /* IP checksum */
    uint16_t ipck = net_ip_checksum(pkt + ip_off, 20);
    pkt[ip_off + 10] = (uint8_t)(ipck & 0xFF);
    pkt[ip_off + 11] = (uint8_t)(ipck >> 8);

    /* ---- TCP (20 byte) ---- */
    uint16_t tcp_off = p;

    pkt[p++] = (uint8_t)(sock->local_port >> 8);   /* src port */
    pkt[p++] = (uint8_t)(sock->local_port);
    pkt[p++] = (uint8_t)(sock->remote_port >> 8);   /* dst port */
    pkt[p++] = (uint8_t)(sock->remote_port);

    pkt[p++] = (uint8_t)(sock->our_seq >> 24);      /* seq */
    pkt[p++] = (uint8_t)(sock->our_seq >> 16);
    pkt[p++] = (uint8_t)(sock->our_seq >> 8);
    pkt[p++] = (uint8_t)(sock->our_seq);

    pkt[p++] = (uint8_t)(sock->our_ack >> 24);      /* ack */
    pkt[p++] = (uint8_t)(sock->our_ack >> 16);
    pkt[p++] = (uint8_t)(sock->our_ack >> 8);
    pkt[p++] = (uint8_t)(sock->our_ack);

    pkt[p++] = 0x50;                                 /* data offset=5 (20 byte) */
    pkt[p++] = flags;                                /* flags */

    pkt[p++] = (uint8_t)(TCP_RX_BUF_SIZE >> 8);     /* window */
    pkt[p++] = (uint8_t)(TCP_RX_BUF_SIZE);

    pkt[p++] = 0x00; pkt[p++] = 0x00;               /* checksum (sonra) */
    pkt[p++] = 0x00; pkt[p++] = 0x00;               /* urgent */

    /* ---- Payload ---- */
    if (payload && payload_len > 0) {
        const uint8_t* src = (const uint8_t*)payload;
        for (uint16_t i = 0; i < payload_len; i++)
            pkt[p++] = src[i];
    }

    /* TCP checksum — pseudo header dahil */
    uint16_t tcp_total_len = tcp_hlen + payload_len;
    uint32_t sum = 0;

    /* Pseudo header (hesapla ama pakete YAZMA) */
    sum += ((uint16_t)nc->our_ip[0] << 8) | nc->our_ip[1];
    sum += ((uint16_t)nc->our_ip[2] << 8) | nc->our_ip[3];
    sum += ((uint16_t)sock->remote_ip[0] << 8) | sock->remote_ip[1];
    sum += ((uint16_t)sock->remote_ip[2] << 8) | sock->remote_ip[3];
    sum += 6;                /* protocol = TCP */
    sum += tcp_total_len;    /* TCP length */

    /* TCP header + data */
    const uint8_t* tcp_bytes = pkt + tcp_off;
    uint16_t tcp_rem = tcp_total_len;
    int idx = 0;
    while (tcp_rem > 1) {
        sum += ((uint16_t)tcp_bytes[idx] << 8) | tcp_bytes[idx + 1];
        idx += 2;
        tcp_rem -= 2;
    }
    if (tcp_rem == 1) {
        sum += ((uint16_t)tcp_bytes[idx]) << 8;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    uint16_t tcp_ck = (uint16_t)(~sum);
    pkt[tcp_off + 16] = (uint8_t)(tcp_ck >> 8);
    pkt[tcp_off + 17] = (uint8_t)(tcp_ck);

    /* Minimum 60 byte */
    uint16_t frame_len = p;
    if (frame_len < 60) frame_len = 60;

    return e1000_send(pkt, frame_len);
}

/* ======== Connect ======== */

int tcp_connect(const uint8_t dst_ip[4], uint16_t dst_port) {
    int sid = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!sockets[i].active) { sid = i; break; }
    }
    if (sid < 0) return -1;

    tcp_socket_t* sock = &sockets[sid];
    k_memset(sock, 0, sizeof(tcp_socket_t));

    ipcpy(sock->remote_ip, dst_ip);
    sock->remote_port = dst_port;
    sock->local_port = next_port++;
    if (next_port > 60000) next_port = 49152;

    sock->our_seq = 1000;
    sock->our_ack = 0;
    sock->active = 1;
    sock->state = TCP_STATE_SYN_SENT;

    /* Gateway ARP — 5 deneme */
    net_config_t* nc = net_get_config();
    uint8_t mac[6];
    for (int r = 0; r < 5; r++) {
        if (net_arp_lookup(nc->gateway_ip, mac)) break;
        net_send_arp_request(nc->gateway_ip);
        for (volatile int w = 0; w < 2000000; w++);
        net_poll();
    }
    if (!net_arp_lookup(nc->gateway_ip, mac)) {
        sock->active = 0;
        return -10;
    }

    /* SYN — 5 deneme */
    for (int attempt = 0; attempt < 5; attempt++) {
        sock->our_seq = 1000;
        sock->our_ack = 0;
        sock->state = TCP_STATE_SYN_SENT;
        sock->error = 0;

        int ret = tcp_send_frame(sock, TCP_SYN, 0, 0);
        if (ret < 0) continue;

        sock->our_seq++;

        /* SYN+ACK bekle — daha uzun */
        for (int w = 0; w < 800; w++) {
            for (volatile int d = 0; d < 200000; d++);
            net_poll();

            if (sock->state == TCP_STATE_ESTABLISHED) {
                sock->connected = 1;
                return sid;
            }
            if (sock->error) break;
        }
    }

    sock->active = 0;
    return -4;
}

/* ======== Handle Incoming ======== */

void tcp_handle_packet(const uint8_t* data, uint16_t len, const uint8_t src_ip[4]) {
    if (len < 20) return;

    uint16_t src_port = ((uint16_t)data[0] << 8) | data[1];
    uint16_t dst_port = ((uint16_t)data[2] << 8) | data[3];
    uint32_t seq = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                   ((uint32_t)data[6] << 8) | data[7];
    uint32_t ack = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
                   ((uint32_t)data[10] << 8) | data[11];
    uint8_t hdr_words = (data[12] >> 4);
    uint16_t hdr_len = (uint16_t)hdr_words * 4;
    uint8_t flags = data[13];

    tcp_socket_t* sock = 0;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (sockets[i].active &&
            sockets[i].local_port == dst_port &&
            sockets[i].remote_port == src_port &&
            ipeq(sockets[i].remote_ip, src_ip)) {
            sock = &sockets[i];
            break;
        }
    }
    if (!sock) return;

    if (flags & TCP_RST) {
        sock->error = 1;
        sock->state = TCP_STATE_CLOSED;
        sock->connected = 0;
        return;
    }

    switch (sock->state) {
        case TCP_STATE_SYN_SENT:
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                sock->their_seq = seq + 1;
                sock->our_ack = seq + 1;
                sock->their_ack = ack;
                sock->state = TCP_STATE_ESTABLISHED;
                tcp_send_frame(sock, TCP_ACK, 0, 0);
            }
            break;

        case TCP_STATE_ESTABLISHED: {
            if (flags & TCP_FIN) {
                sock->our_ack = seq + 1;
                sock->state = TCP_STATE_CLOSE_WAIT;
                tcp_send_frame(sock, TCP_ACK, 0, 0);
                sock->connected = 0;
                break;
            }

            if (flags & TCP_ACK) {
                sock->their_ack = ack;
            }

            if (hdr_len >= 20 && hdr_len <= len) {
                uint16_t pload_len = len - hdr_len;
                if (pload_len > 0) {
                    const uint8_t* pload = data + hdr_len;

                    if (seq >= sock->our_ack) {
                        uint16_t space = TCP_RX_BUF_SIZE - sock->rx_len;
                        uint16_t copy = pload_len > space ? space : pload_len;

                        for (uint16_t i = 0; i < copy; i++)
                            sock->rx_buf[sock->rx_len++] = pload[i];

                        sock->our_ack = seq + copy;
                        sock->data_ready = 1;
                        tcp_send_frame(sock, TCP_ACK, 0, 0);
                    }
                }
            }
            break;
        }

        case TCP_STATE_FIN_WAIT:
            if (flags & TCP_FIN) {
                sock->our_ack = seq + 1;
                tcp_send_frame(sock, TCP_ACK, 0, 0);
            }
            if (flags & TCP_ACK) {
                sock->state = TCP_STATE_CLOSED;
                sock->connected = 0;
                sock->active = 0;
            }
            break;

        default:
            break;
    }
}

/* ======== Send ======== */

int tcp_send(int sid, const void* data, uint16_t len) {
    if (sid < 0 || sid >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t* sock = &sockets[sid];
    if (!sock->active || sock->state != TCP_STATE_ESTABLISHED) return -2;

    const uint8_t* ptr = (const uint8_t*)data;
    uint16_t remaining = len;

    while (remaining > 0) {
        uint16_t chunk = remaining > 1400 ? 1400 : remaining;
        int ret = tcp_send_frame(sock, TCP_PSH | TCP_ACK, ptr, chunk);
        if (ret < 0) return ret;
        sock->our_seq += chunk;
        ptr += chunk;
        remaining -= chunk;

        for (int w = 0; w < 30; w++) {
            for (volatile int d = 0; d < 100000; d++);
            net_poll();
            if (sock->their_ack >= sock->our_seq) break;
        }
    }
    return (int)len;
}

/* ======== Recv ======== */

int tcp_recv(int sid, void* buffer, uint16_t max_len) {
    if (sid < 0 || sid >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t* sock = &sockets[sid];
    if (!sock->active && sock->rx_len == 0) return -2;
    if (sock->rx_len == 0) return 0;

    uint16_t copy = sock->rx_len > max_len ? max_len : sock->rx_len;
    uint8_t* dst = (uint8_t*)buffer;
    for (uint16_t i = 0; i < copy; i++) dst[i] = sock->rx_buf[i];

    uint16_t rem = sock->rx_len - copy;
    for (uint16_t i = 0; i < rem; i++)
        sock->rx_buf[i] = sock->rx_buf[copy + i];
    sock->rx_len = rem;
    if (!sock->rx_len) sock->data_ready = 0;
    return (int)copy;
}

/* ======== Close ======== */

int tcp_close(int sid) {
    if (sid < 0 || sid >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t* sock = &sockets[sid];
    if (!sock->active) return 0;

    if (sock->state == TCP_STATE_ESTABLISHED) {
        tcp_send_frame(sock, TCP_FIN | TCP_ACK, 0, 0);
        sock->our_seq++;
        sock->state = TCP_STATE_FIN_WAIT;
        for (int w = 0; w < 50; w++) {
            for (volatile int d = 0; d < 200000; d++);
            net_poll();
            if (sock->state == TCP_STATE_CLOSED) break;
        }
    }
    sock->active = 0;
    sock->connected = 0;
    sock->state = TCP_STATE_CLOSED;
    return 0;
}

int tcp_is_connected(int sid) {
    if (sid < 0 || sid >= TCP_MAX_SOCKETS) return 0;
    return sockets[sid].connected;
}
int tcp_has_data(int sid) {
    if (sid < 0 || sid >= TCP_MAX_SOCKETS) return 0;
    return sockets[sid].rx_len > 0;
}
int tcp_get_error(int sid) {
    if (sid < 0 || sid >= TCP_MAX_SOCKETS) return -1;
    return sockets[sid].error;
}
