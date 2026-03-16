#include "http.h"
#include "tcp.h"
#include "dns.h"
#include "net.h"
#include "string.h"

void http_init(void) {
    /* Ileride cookie/cache icin */
}

/* Basit sayi parse */
static int parse_int(const char* s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

/* Header'da deger bul */
static int find_header(const char* data, int len, const char* name, char* out, int max) {
    int nlen = k_strlen(name);
    for (int i = 0; i < len - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            char a = data[i + j];
            char b = name[j];
            /* Case insensitive */
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = 0; break; }
        }
        if (match) {
            int start = i + nlen;
            /* Bosluklari atla */
            while (start < len && data[start] == ' ') start++;
            int end = start;
            while (end < len && data[end] != '\r' && data[end] != '\n') end++;
            int copy = end - start;
            if (copy >= max) copy = max - 1;
            for (int j = 0; j < copy; j++) out[j] = data[start + j];
            out[copy] = '\0';
            return 1;
        }
    }
    return 0;
}

/* Body baslangicini bul (\r\n\r\n) */
static int find_body(const char* data, int len) {
    for (int i = 0; i < len - 3; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' &&
            data[i+2] == '\r' && data[i+3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

/* Status code parse: "HTTP/1.x 200 OK" */
static int parse_status(const char* data, int len) {
    /* "HTTP/" bul */
    for (int i = 0; i < len - 12; i++) {
        if (data[i]=='H' && data[i+1]=='T' && data[i+2]=='T' && data[i+3]=='P' && data[i+4]=='/') {
            /* Bosluktan sonra sayi */
            int j = i + 5;
            while (j < len && data[j] != ' ') j++;
            j++; /* Bosluk atla */
            if (j < len)
                return parse_int(data + j);
        }
    }
    return 0;
}

/* ======== HTTP GET ======== */

int http_get(const char* host, const char* path, http_response_t* resp) {
    k_memset(resp, 0, sizeof(http_response_t));

    /* DNS cozumle */
    uint8_t server_ip[4];
    int dns_ret = dns_resolve(host, server_ip);
    if (dns_ret != 0) {
        resp->error = -1;
        return -1;
    }

    /* TCP baglan */
    int sock = tcp_connect(server_ip, 80);
    if (sock < 0) {
        resp->error = -2;
        return -2;
    }

    /* HTTP request olustur */
    char request[512];
    k_memset(request, 0, sizeof(request));

    k_strcpy(request, "GET ");
    k_strcpy(request + k_strlen(request), path);
    k_strcpy(request + k_strlen(request), " HTTP/1.0\r\nHost: ");
    k_strcpy(request + k_strlen(request), host);
    k_strcpy(request + k_strlen(request), "\r\nUser-Agent: MyOS/0.3\r\nAccept: text/html\r\nConnection: close\r\n\r\n");

    /* Gonder */
    int slen = k_strlen(request);
    int ret = tcp_send(sock, request, (uint16_t)slen);
    if (ret < 0) {
        tcp_close(sock);
        resp->error = -3;
        return -3;
    }

    /* Cevap al */
    uint8_t raw[4096];
    int total = 0;

    for (int w = 0; w < 500; w++) {
        for (volatile int d = 0; d < 200000; d++);
        net_poll();

        int rlen = tcp_recv(sock, raw + total, (uint16_t)(sizeof(raw) - 1 - total));
        if (rlen > 0) {
            total += rlen;
            if (total >= (int)sizeof(raw) - 1) break;

            /* Body tamam mi kontrol */
            raw[total] = '\0';
            int body_off = find_body((char*)raw, total);
            if (body_off > 0) {
                /* Content-Length kontrol */
                char cl_str[16];
                if (find_header((char*)raw, total, "content-length:", cl_str, 16)) {
                    int cl = parse_int(cl_str);
                    if (total - body_off >= cl) break;
                }
            }
        }

        if (!tcp_is_connected(sock) && total > 0) break;
    }

    raw[total] = '\0';
    tcp_close(sock);

    if (total == 0) {
        resp->error = -4;
        return -4;
    }

    /* Response parse */
    resp->status_code = parse_status((char*)raw, total);

    find_header((char*)raw, total, "content-type:", resp->content_type, 64);

    char cl_str2[16];
    if (find_header((char*)raw, total, "content-length:", cl_str2, 16)) {
        resp->content_length = parse_int(cl_str2);
    }

    /* Body */
    int body_start = find_body((char*)raw, total);
    if (body_start > 0 && body_start < total) {
        int blen = total - body_start;
        if (blen > HTTP_MAX_BODY - 1) blen = HTTP_MAX_BODY - 1;
        for (int i = 0; i < blen; i++)
            resp->body[i] = (char)raw[body_start + i];
        resp->body[blen] = '\0';
        resp->body_len = blen;
    }

    return 0;
}
