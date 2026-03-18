#ifndef DNS_H
#define DNS_H

#include <stdint.h>

#define DNS_PORT       53
#define DNS_LOCAL_PORT 12345

/* DNS sonucu */
typedef struct {
    uint8_t ip[4];
    int     resolved;
    int     error;
} dns_result_t;

int  dns_resolve(const char* hostname, uint8_t ip_out[4]);
void dns_init(void);

#endif
