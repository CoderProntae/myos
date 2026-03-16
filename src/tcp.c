#include "tcp.h"
#include "net.h"
#include "e1000.h"
#include "string.h"
#include "io.h"

static tcp_socket_t sockets[TCP_MAX_SOCKETS];
static uint16_t next_local_port = 49152;

/* Pseudo header for TCP checksum */
typedef struct {
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_length;
} __attribute__((packed)) tcp_pseudo_t;

static void ip_copy(uint8_t* d, const uint8_t* s) {
    for (int i = 0; i < 4; i++) d[i] = s[i];
}

static int ip_eq(const uint8_t* a, const uint8_t* b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

/* ======== Init ======== */

void tcp_init(void) {
    k_memset(sockets, 0, sizeof(sockets));
    next_local_port = 49152;
}

/* ======== Checksum ======== */

static uint16_t tcp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                              const void* tcp_data, uint16_t tcp_len) {
    tcp_pseudo_t pseudo;
    ip_copy(pseudo.src_ip, src_ip);
    ip_copy(pseudo.dst_ip, dst_ip);
    pseudo.zero = 0;
    pseudo.protocol = IP_PROTO_TCP;
    pseudo.tcp_length = htons(tcp_len);

    uint32_t sum = 0;
    const uint16_t* p;

    /* Pseudo header */
    p = (const uint16_t*)&pseudo;
    for (int i = 0; i < 6; i++) sum += p[i];

    /* TCP data */
    p = (const uint16_t*)tcp_data;
    int remaining = tcp_len;
    while (remaining > 1) {
        sum += *p++;
        remaining -= 2;
    }
    if (remaining == 1) {
        sum += *(const uint8_t*)p;
    }

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* ======== TCP Segment Gonder ======== */

static int tcp_send_segment(tcp_socket_t* sock, uint8_t flags,
                             const void* data, uint16_t data_len) {
    net_config_t* nc = net_get_config();
    if (!nc->configured) return -1;

    uint16_t tcp_len = 20 + data_len;  /* TCP header = 20 byte (no options) */
    uint8_t buf[2048];

    tcp_header_t* tcp = (tcp_header_t*)buf;
    tcp->src_port    = htons(sock->local_port);
    tcp->dst_port    = htons(sock->remote_port);
    tcp->seq_num     = htonl(sock->our_seq);
    tcp->ack_num     = htonl(sock->our_ack);
    tcp->data_offset = (5 << 4);  /* 5 * 4 = 20 bytes */
    tcp->flags       = flags;
    tcp->window      = htons(TCP_RX_BUF_SIZE);
    tcp->checksum    = 0;
    tcp->urgent      = 0;

    /* Veri kopyala */
    if (data && data_len > 0) {
        const uint8_t* src = (const uint8_t*)data;
        for (uint16_t i = 0; i < data_len; i++)
            buf[20 + i] = src[i];
    }

    /* Checksum hesapla */
    tcp->checksum = tcp_checksum(nc->our_ip, sock->remote_ip, buf, tcp_len);

    /* IP ile gonder */
    /* send_ip net.c'de static, bu yuzden net_send_udp benzeri bir fonksiyon lazim */
    /* Workaround: raw IP paketi olustur */

    /* IP + TCP */
    uint8_t ip_buf[2048];
    ip_header_t* ip = (ip_header_t*)ip_buf;
    ip->ver_ihl      = 0x45;
    ip->tos           = 0;
    ip->total_length  = htons(20 + tcp_len);
    ip->id            = htons(next_local_port++); /* basit ID */
    ip->flags_frag    = 0;
    ip->ttl           = 64;
    ip->protocol      = IP_PROTO_TCP;
    ip->checksum      = 0;
    ip_copy(ip->src_ip, nc->our_ip);
    ip_copy(ip->dst_ip, sock->remote_ip);
    ip->checksum = net_ip_checksum(ip, 20);

    /* TCP verisini ekle */
    for (uint16_t i = 0; i < tcp_len; i++)
        ip_buf[20 + i] = buf[i];

    /* Ethernet frame */
    uint8_t dst_mac[6];
    if (!net_arp_lookup(sock->remote_ip, dst_mac)) {
        /* Gateway MAC dene */
        net_config_t* nc2 = net_get_config();
        if (!net_arp_lookup(nc2->gateway_ip, dst_mac)) {
            /* ARP request gonder ve bekle */
            net_send_arp_request(nc2->gateway_ip);
            for (volatile int w = 0; w < 1000000; w++);
            net_poll();
            if (!net_arp_lookup(nc2->gateway_ip, dst_mac)) {
                k_memset(dst_mac, 0xFF, 6);
            }
        }
    }

    /* Ethernet frame olustur */
    uint8_t frame[2048];
    uint8_t our_mac[6];
    e1000_get_mac(our_mac);

    /* Eth header */
    for (int i = 0; i < 6; i++) frame[i] = dst_mac[i];
    for (int i = 0; i < 6; i++) frame[6 + i] = our_mac[i];
    frame[12] = 0x08;  /* ETH_TYPE_IP = 0x0800 */
    frame[13] = 0x00;

    /* IP + TCP */
    uint16_t ip_total = 20 + tcp_len;
    for (uint16_t i = 0; i < ip_total; i++)
        frame[14 + i] = ip_buf[i];

    uint16_t frame_len = 14 + ip_total;
    if (frame_len < 60) {
        for (uint16_t i = frame_len; i < 60; i++) frame[i] = 0;
        frame_len = 60;
    }

    return e1000_send(frame, frame_len);
}

/* ======== Connect (3-way handshake) ======== */

int tcp_connect(const uint8_t dst_ip[4], uint16_t dst_port) {
    /* Bos soket bul */
    int sock_id = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!sockets[i].active) {
            sock_id = i;
            break;
        }
    }
    if (sock_id < 0) return -1;

    tcp_socket_t* sock = &sockets[sock_id];
    k_memset(sock, 0, sizeof(tcp_socket_t));

    ip_copy(sock->remote_ip, dst_ip);
    sock->remote_port = dst_port;
    sock->local_port  = next_local_port++;
    if (next_local_port > 60000) next_local_port = 49152;

    /* ISN (Initial Sequence Number) — basit */
    sock->our_seq = 1000 + sock_id * 1000;
    sock->our_ack = 0;
    sock->active = 1;
    sock->connected = 0;
    sock->error = 0;
    sock->state = TCP_STATE_SYN_SENT;

    /* Gateway ARP'sini onceden cozumle */
    net_config_t* nc = net_get_config();
    uint8_t dummy_mac[6];
    if (!net_arp_lookup(nc->gateway_ip, dummy_mac)) {
        net_send_arp_request(nc->gateway_ip);
        for (volatile int w = 0; w < 1000000; w++);
        net_poll();
    }

    /* SYN gonder */
    int ret = tcp_send_segment(sock, TCP_SYN, 0, 0);
    if (ret < 0) {
        sock->active = 0;
        return -2;
    }
    sock->our_seq++;  /* SYN 1 byte tuketir */

    /* SYN+ACK bekle */
    for (int attempt = 0; attempt < 300; attempt++) {
        for (volatile int w = 0; w < 300000; w++);
        net_poll();

        if (sock->state == TCP_STATE_ESTABLISHED) {
            sock->connected = 1;
            return sock_id;
        }
        if (sock->error) {
            sock->active = 0;
            return -3;
        }
    }

    /* Timeout — tekrar dene */
    sock->our_seq--;
    ret = tcp_send_segment(sock, TCP_SYN, 0, 0);
    if (ret < 0) { sock->active = 0; return -2; }
    sock->our_seq++;

    for (int attempt = 0; attempt < 500; attempt++) {
        for (volatile int w = 0; w < 300000; w++);
        net_poll();
        if (sock->state == TCP_STATE_ESTABLISHED) {
            sock->connected = 1;
            return sock_id;
        }
        if (sock->error) { sock->active = 0; return -3; }
    }

    sock->active = 0;
    return -4;  /* Timeout */
}

/* ======== Gelen TCP Paketi Isle ======== */

void tcp_handle_packet(const uint8_t* data, uint16_t len, const uint8_t src_ip[4]) {
    if (len < 20) return;

    const tcp_header_t* tcp = (const tcp_header_t*)data;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint32_t seq      = ntohl(tcp->seq_num);
    uint32_t ack      = ntohl(tcp->ack_num);
    uint8_t  flags    = tcp->flags;
    uint16_t hdr_len  = (tcp->data_offset >> 4) * 4;

    /* Hangi soket? */
    tcp_socket_t* sock = 0;
    int sock_id = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (sockets[i].active &&
            sockets[i].local_port == dst_port &&
            sockets[i].remote_port == src_port &&
            ip_eq(sockets[i].remote_ip, src_ip)) {
            sock = &sockets[i];
            sock_id = i;
            break;
        }
    }
    if (!sock) return;

    /* RST */
    if (flags & TCP_RST) {
        sock->error = 1;
        sock->state = TCP_STATE_CLOSED;
        sock->connected = 0;
        return;
    }

    /* State machine */
    switch (sock->state) {
        case TCP_STATE_SYN_SENT:
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                /* SYN+ACK geldi */
                sock->their_seq = seq + 1;
                sock->our_ack = seq + 1;
                sock->their_ack = ack;
                sock->state = TCP_STATE_ESTABLISHED;

                /* ACK gonder */
                tcp_send_segment(sock, TCP_ACK, 0, 0);
            }
            break;

        case TCP_STATE_ESTABLISHED:
            if (flags & TCP_FIN) {
                /* Karsi taraf kapatmak istiyor */
                sock->our_ack = seq + 1;
                sock->state = TCP_STATE_CLOSE_WAIT;
                tcp_send_segment(sock, TCP_ACK, 0, 0);
                sock->connected = 0;
                break;
            }

            if (flags & TCP_ACK) {
                sock->their_ack = ack;
            }

            /* Veri var mi? */
            uint16_t payload_len = len - hdr_len;
            if (payload_len > 0 && hdr_len <= len) {
                const uint8_t* payload = data + hdr_len;

                /* Beklenen seq mi? */
                if (seq == sock->our_ack) {
                    /* RX buffer'a kopyala */
                    uint16_t space = TCP_RX_BUF_SIZE - sock->rx_len;
                    uint16_t copy = payload_len;
                    if (copy > space) copy = space;

                    for (uint16_t i = 0; i < copy; i++)
                        sock->rx_buf[sock->rx_len++] = payload[i];

                    sock->our_ack += copy;
                    sock->data_ready = 1;

                    /* ACK gonder */
                    tcp_send_segment(sock, TCP_ACK, 0, 0);
                }
            }
            break;

        case TCP_STATE_FIN_WAIT:
            if (flags & TCP_ACK) {
                sock->state = TCP_STATE_TIME_WAIT;
                sock->connected = 0;
                sock->active = 0;
            }
            if (flags & TCP_FIN) {
                sock->our_ack = seq + 1;
                tcp_send_segment(sock, TCP_ACK, 0, 0);
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

int tcp_send(int sock_id, const void* data, uint16_t len) {
    if (sock_id < 0 || sock_id >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t* sock = &sockets[sock_id];
    if (!sock->active || sock->state != TCP_STATE_ESTABLISHED) return -2;

    /* Parcalara bol (MSS = 1460) */
    const uint8_t* ptr = (const uint8_t*)data;
    uint16_t remaining = len;
    uint16_t mss = 1400;

    while (remaining > 0) {
        uint16_t chunk = remaining > mss ? mss : remaining;

        int ret = tcp_send_segment(sock, TCP_PSH | TCP_ACK, ptr, chunk);
        if (ret < 0) return ret;

        sock->our_seq += chunk;
        ptr += chunk;
        remaining -= chunk;

        /* ACK bekle */
        for (int w = 0; w < 50; w++) {
            for (volatile int d = 0; d < 100000; d++);
            net_poll();
            if (sock->their_ack >= sock->our_seq) break;
        }
    }

    return (int)len;
}

/* ======== Recv ======== */

int tcp_recv(int sock_id, void* buffer, uint16_t max_len) {
    if (sock_id < 0 || sock_id >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t* sock = &sockets[sock_id];
    if (!sock->active) return -2;

    if (sock->rx_len == 0) return 0;

    uint16_t copy = sock->rx_len;
    if (copy > max_len) copy = max_len;

    uint8_t* dst = (uint8_t*)buffer;
    for (uint16_t i = 0; i < copy; i++)
        dst[i] = sock->rx_buf[i];

    /* Kalan veriyi basa kaydir */
    uint16_t remaining = sock->rx_len - copy;
    for (uint16_t i = 0; i < remaining; i++)
        sock->rx_buf[i] = sock->rx_buf[copy + i];
    sock->rx_len = remaining;

    if (sock->rx_len == 0)
        sock->data_ready = 0;

    return (int)copy;
}

/* ======== Close ======== */

int tcp_close(int sock_id) {
    if (sock_id < 0 || sock_id >= TCP_MAX_SOCKETS) return -1;
    tcp_socket_t* sock = &sockets[sock_id];
    if (!sock->active) return 0;

    if (sock->state == TCP_STATE_ESTABLISHED) {
        /* FIN gonder */
        tcp_send_segment(sock, TCP_FIN | TCP_ACK, 0, 0);
        sock->our_seq++;
        sock->state = TCP_STATE_FIN_WAIT;

        /* ACK bekle */
        for (int w = 0; w < 100; w++) {
            for (volatile int d = 0; d < 200000; d++);
            net_poll();
            if (sock->state == TCP_STATE_CLOSED ||
                sock->state == TCP_STATE_TIME_WAIT) break;
        }
    }

    sock->active = 0;
    sock->connected = 0;
    sock->state = TCP_STATE_CLOSED;
    return 0;
}

/* ======== Yardimci ======== */

int tcp_is_connected(int sock_id) {
    if (sock_id < 0 || sock_id >= TCP_MAX_SOCKETS) return 0;
    return sockets[sock_id].connected;
}

int tcp_has_data(int sock_id) {
    if (sock_id < 0 || sock_id >= TCP_MAX_SOCKETS) return 0;
    return sockets[sock_id].rx_len > 0;
}

int tcp_get_error(int sock_id) {
    if (sock_id < 0 || sock_id >= TCP_MAX_SOCKETS) return -1;
    return sockets[sock_id].error;
}
