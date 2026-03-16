#include "http.h"
#include "tcp.h"
#include "dns.h"
#include "net.h"
#include "string.h"

void http_init(void) {
    /* Bos — ileride cookie/cache eklenebilir */
}

/* Basit header parser */
static void parse_headers(const char* raw, int raw_len, http_response_t* resp) {
    /* Status line: "HTTP/1.x NNN ..." */
    resp->status_code = 0;
    resp->content_length = -1;
    resp->content_type[0] = '\0';

    const char* p = raw;
    const char* end = raw + raw_len;

    /* Status code */
    while (p < end && *p != ' ') p++;
    if (p < end) p++;
    if (p + 3 <= end) {
        resp->status_code = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
    }

    /* Header satirlarini tara */
    while (p < end) {
        /* Satir basina git */
        while (p < end && *p != '\n') p++;
        if (p < end) p++;

        /* Bos satir = header sonu */
        if (p < end && (*p == '\r' || *p == '\n')) break;

        /* Content-Type */
        if (p + 13 < end && k_strncmp(p, "Content-Type:", 13) == 0) {
            const char* val = p + 13;
            while (val < end && *val == ' ') val++;
            int i = 0;
            while (val < end && *val != '\r' && *val != '\n' && i < 63) {
                resp->content_type[i++] = *val++;
            }
            resp->content_type[i] = '\0';
        }

        /* content-type (kucuk harf) */
        if (p + 13 < end && k_strncmp(p, "content-type:", 13) == 0) {
            const char* val = p + 13;
            while (val < end && *val == ' ') val++;
            int i = 0;
            while (val < end && *val != '\r' && *val != '\n' && i < 63) {
                resp->content_type[i++] = *val++;
            }
            resp->content_type[i] = '\0';
        }

        /* Content-Length */
        if (p + 15 < end && k_strncmp(p, "Content-Length:", 15) == 0) {
            const char* val = p + 15;
            while (val < end && *val == ' ') val++;
            int num = 0;
            while (val < end && *val >= '0' && *val <= '9') {
                num = num * 10 + (*val - '0');
                val++;
            }
            resp->content_length = num;
        }
        if (p + 15 < end && k_strncmp(p, "content-length:", 15) == 0) {
            const char* val = p + 15;
            while (val < end && *val == ' ') val++;
            int num = 0;
            while (val < end && *val >= '0' && *val <= '9') {
                num = num * 10 + (*val - '0');
                val++;
            }
            resp->content_length = num;
        }
    }
}

/* Header/Body ayirma */
static int find_body_start(const char* data, int len) {
    for (int i = 0; i < len - 3; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' &&
            data[i+2] == '\r' && data[i+3] == '\n') {
            return i + 4;
        }
    }
    /* \n\n de dene */
    for (int i = 0; i < len - 1; i++) {
        if (data[i] == '\n' && data[i+1] == '\n') {
            return i + 2;
        }
    }
    return -1;
}

/* ======== HTTP GET ======== */

int http_get(const char* host, const char* path, http_response_t* resp) {
    k_memset(resp, 0, sizeof(http_response_t));

    /* 1. DNS cozumle */
    uint8_t server_ip[4];
    int dns_ret = dns_resolve(host, server_ip);
    if (dns_ret != 0) {
        k_strcpy(resp->error_msg, "DNS cozumlenemedi");
        resp->error = -1;
        return -1;
    }

    /* 2. TCP baglan */
    int sock = tcp_connect(server_ip, 80);
    if (sock < 0) {
        k_strcpy(resp->error_msg, "TCP baglanti basarisiz");
        resp->error = -2;
        return -2;
    }

    /* 3. HTTP request olustur */
    char request[512];
    k_memset(request, 0, sizeof(request));

    k_strcpy(request, "GET ");
    k_strcpy(request + k_strlen(request), path);
    k_strcpy(request + k_strlen(request), " HTTP/1.0\r\nHost: ");
    k_strcpy(request + k_strlen(request), host);
    k_strcpy(request + k_strlen(request), "\r\nUser-Agent: MyOS/0.3\r\nAccept: */*\r\nConnection: close\r\n\r\n");

    /* 4. Request gonder */
    int req_len = k_strlen(request);
    int ret = tcp_send(sock, request, (uint16_t)req_len);
    if (ret < 0) {
        k_strcpy(resp->error_msg, "HTTP gonderme basarisiz");
        resp->error = -3;
        tcp_close(sock);
        return -3;
    }

    /* 5. Response al */
    char raw[HTTP_MAX_BODY + HTTP_MAX_HEADER];
    int total = 0;
    int max_raw = (int)sizeof(raw) - 1;

    for (int w = 0; w < 500; w++) {
        for (volatile int d = 0; d < 200000; d++);
        net_poll();

        int space = max_raw - total;
        if (space <= 0) break;

        int rlen = tcp_recv(sock, raw + total, (uint16_t)space);
        if (rlen > 0) {
            total += rlen;
            w = 0;  /* Veri geldikce timer'i sifirla */
        }

        if (!tcp_is_connected(sock) && rlen <= 0 && total > 0)
            break;
    }

    raw[total] = '\0';
    tcp_close(sock);

    if (total == 0) {
        k_strcpy(resp->error_msg, "Cevap alinamadi");
        resp->error = -4;
        return -4;
    }

    /* 6. Header parse */
    parse_headers(raw, total, resp);

    /* 7. Body ayir */
    int body_start = find_body_start(raw, total);
    if (body_start >= 0 && body_start < total) {
        resp->body_len = total - body_start;
        if (resp->body_len > HTTP_MAX_BODY - 1)
            resp->body_len = HTTP_MAX_BODY - 1;
        for (int i = 0; i < resp->body_len; i++)
            resp->body[i] = raw[body_start + i];
        resp->body[resp->body_len] = '\0';
    }

    return 0;
}
