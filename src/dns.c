#include "dns.h"
#include "net.h"
#include "string.h"

/* DNS Header */
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

static uint16_t dns_id = 0x1000;

void dns_init(void) {
    dns_id = 0x1000;
}

/* Hostname'i DNS label formatina cevir */
/* "www.google.com" -> "\3www\6google\3com\0" */
static int encode_name(const char* name, uint8_t* out) {
    int pos = 0;
    const char* p = name;

    while (*p) {
        /* Noktaya kadar olan kismi say */
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

    out[pos++] = 0;  /* Sonlandirici */
    return pos;
}

/* DNS cevabindan IP adresini parse et */
static int parse_response(const uint8_t* data, int len, uint8_t ip_out[4]) {
    if (len < (int)sizeof(dns_header_t)) return -1;

    const dns_header_t* hdr = (const dns_header_t*)data;
    uint16_t flags   = ntohs(hdr->flags);
    uint16_t ancount = ntohs(hdr->ancount);

    /* Hata kontrolu: RCODE (alt 4 bit) */
    if (flags & 0x000F) return -2;

    /* Cevap yok */
    if (ancount == 0) return -3;

    /* Question bölümünü atla */
    int pos = sizeof(dns_header_t);
    uint16_t qdcount = ntohs(hdr->qdcount);

    for (uint16_t q = 0; q < qdcount; q++) {
        /* Label'lari atla */
        while (pos < len) {
            uint8_t label_len = data[pos];
            if (label_len == 0) { pos++; break; }
            if ((label_len & 0xC0) == 0xC0) { pos += 2; break; }
            pos += 1 + label_len;
        }
        pos += 4;  /* QTYPE(2) + QCLASS(2) */
    }

    /* Answer bolumu */
    for (uint16_t a = 0; a < ancount && pos < len; a++) {
        /* Name — pointer veya label */
        if (pos >= len) break;
        if ((data[pos] & 0xC0) == 0xC0) {
            pos += 2;  /* Pointer: 2 byte */
        } else {
            while (pos < len) {
                uint8_t ll = data[pos];
                if (ll == 0) { pos++; break; }
                if ((ll & 0xC0) == 0xC0) { pos += 2; break; }
                pos += 1 + ll;
            }
        }

        if (pos + 10 > len) break;

        uint16_t rtype  = ((uint16_t)data[pos] << 8) | data[pos+1];
        /* uint16_t rclass = ((uint16_t)data[pos+2] << 8) | data[pos+3]; */
        /* uint32_t ttl (4 byte) */
        uint16_t rdlen  = ((uint16_t)data[pos+8] << 8) | data[pos+9];
        pos += 10;

        /* A record (type 1) = IPv4 adresi */
        if (rtype == 1 && rdlen == 4 && pos + 4 <= len) {
            ip_out[0] = data[pos];
            ip_out[1] = data[pos+1];
            ip_out[2] = data[pos+2];
            ip_out[3] = data[pos+3];
            return 0;  /* Basarili! */
        }

        pos += rdlen;
    }

    return -4;  /* A record bulunamadi */
}

/* ======== Ana DNS Cozumleme ======== */

int dns_resolve(const char* hostname, uint8_t ip_out[4]) {
    net_config_t* nc = net_get_config();
    if (!nc->configured) return -1;

    /* DNS sorgu paketi olustur */
    uint8_t query[256];
    int qpos = 0;

    /* Header */
    dns_header_t* hdr = (dns_header_t*)query;
    hdr->id      = htons(dns_id++);
    hdr->flags   = htons(0x0100);  /* Standard query, recursion desired */
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;
    qpos = sizeof(dns_header_t);

    /* Question: hostname */
    int name_len = encode_name(hostname, query + qpos);
    if (name_len < 0) return -2;
    qpos += name_len;

    /* QTYPE = A (1) */
    query[qpos++] = 0;
    query[qpos++] = 1;

    /* QCLASS = IN (1) */
    query[qpos++] = 0;
    query[qpos++] = 1;

    /* UDP ile DNS sunucusuna gonder */
    int ret = net_send_udp(nc->dns_ip, DNS_LOCAL_PORT, DNS_PORT,
                           query, (uint16_t)qpos);
    if (ret < 0) return -3;

    /* Cevap bekle — polling */
    uint8_t response[512];
    for (int attempt = 0; attempt < 100; attempt++) {
        for (volatile int w = 0; w < 500000; w++);

        /* Gelen paketleri isle */
        net_poll();

        /* UDP cevabini kontrol et */
        int rlen = net_recv_udp(DNS_LOCAL_PORT, response, sizeof(response));
        if (rlen > 0) {
            /* Parse et */
            int result = parse_response(response, rlen, ip_out);
            if (result == 0) return 0;  /* Basarili! */
            return result;
        }
    }

    return -5;  /* Timeout */
}
