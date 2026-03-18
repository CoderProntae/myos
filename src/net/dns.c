#include "dns.h"
#include "net.h"
#include "string.h"

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

static uint16_t dns_id_counter = 0x1000;
static uint16_t local_port = 10000;

void dns_init(void) {
    dns_id_counter = 0x1000;
    local_port = 10000;
}

static int encode_name(const char* name, uint8_t* out) {
    int pos = 0;
    const char* p = name;
    while (*p) {
        const char* dot = p;
        int len = 0;
        while (*dot && *dot != '.') { dot++; len++; }
        if (len == 0 || len > 63) return -1;
        out[pos++] = (uint8_t)len;
        for (int i = 0; i < len; i++)
            out[pos++] = (uint8_t)p[i];
        p = dot;
        if (*p == '.') p++;
    }
    out[pos++] = 0;
    return pos;
}

static int parse_response(const uint8_t* data, int len, uint16_t expected_id, uint8_t ip_out[4]) {
    if (len < (int)sizeof(dns_header_t)) return -1;

    const dns_header_t* hdr = (const dns_header_t*)data;

    /* ID kontrol */
    if (ntohs(hdr->id) != expected_id) return -10;

    uint16_t flags = ntohs(hdr->flags);

    /* QR bit (bit 15) = 1 olmali (response) */
    if (!(flags & 0x8000)) return -11;

    /* RCODE (alt 4 bit) = 0 olmali (no error) */
    if (flags & 0x000F) return -2;

    uint16_t ancount = ntohs(hdr->ancount);
    if (ancount == 0) return -3;

    /* Question bolumunu atla */
    int pos = (int)sizeof(dns_header_t);
    uint16_t qdcount = ntohs(hdr->qdcount);

    for (uint16_t q = 0; q < qdcount; q++) {
        while (pos < len) {
            uint8_t ll = data[pos];
            if (ll == 0) { pos++; break; }
            if ((ll & 0xC0) == 0xC0) { pos += 2; break; }
            pos += 1 + ll;
        }
        pos += 4;
    }

    /* Answer bolumu */
    for (uint16_t a = 0; a < ancount && pos < len; a++) {
        /* Name */
        if (pos >= len) break;
        if ((data[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (pos < len) {
                uint8_t ll = data[pos];
                if (ll == 0) { pos++; break; }
                if ((ll & 0xC0) == 0xC0) { pos += 2; break; }
                pos += 1 + ll;
            }
        }

        if (pos + 10 > len) break;

        uint16_t rtype = ((uint16_t)data[pos] << 8) | data[pos + 1];
        uint16_t rdlen = ((uint16_t)data[pos + 8] << 8) | data[pos + 9];
        pos += 10;

        if (rtype == 1 && rdlen == 4 && pos + 4 <= len) {
            ip_out[0] = data[pos];
            ip_out[1] = data[pos + 1];
            ip_out[2] = data[pos + 2];
            ip_out[3] = data[pos + 3];
            return 0;
        }

        pos += rdlen;
    }

    return -4;
}

int dns_resolve(const char* hostname, uint8_t ip_out[4]) {
    net_config_t* nc = net_get_config();
    if (!nc->configured) return -1;

    /* Her sorguda farkli port kullan */
    uint16_t src_port = local_port++;
    if (local_port > 60000) local_port = 10000;

    uint16_t qid = dns_id_counter++;

    /* DNS sorgu paketi */
    uint8_t query[256];
    int qpos = 0;

    dns_header_t* hdr = (dns_header_t*)query;
    hdr->id      = htons(qid);
    hdr->flags   = htons(0x0100);  /* Standard query, RD=1 */
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;
    qpos = (int)sizeof(dns_header_t);

    int name_len = encode_name(hostname, query + qpos);
    if (name_len < 0) return -2;
    qpos += name_len;

    /* QTYPE = A (1) */
    query[qpos++] = 0;
    query[qpos++] = 1;
    /* QCLASS = IN (1) */
    query[qpos++] = 0;
    query[qpos++] = 1;

    /* 3 deneme yap */
    for (int retry = 0; retry < 3; retry++) {
        /* Gonder */
        int ret = net_send_udp(nc->dns_ip, src_port, DNS_PORT,
                               query, (uint16_t)qpos);
        if (ret < 0) continue;

        /* Cevap bekle */
        uint8_t response[512];
        for (int attempt = 0; attempt < 200; attempt++) {
            for (volatile int w = 0; w < 300000; w++);
            net_poll();

            int rlen = net_recv_udp(src_port, response, (uint16_t)sizeof(response));
            if (rlen > 0) {
                int result = parse_response(response, rlen, qid, ip_out);
                if (result == 0) return 0;
                /* Yanlis ID ise devam et */
                if (result == -10) continue;
                /* Diger hatalarda da devam et — belki baska paket gelir */
            }
        }
    }

    return -5;
}
