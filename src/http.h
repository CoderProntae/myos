#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

#define HTTP_MAX_BODY 3072

typedef struct {
    int      status_code;
    char     content_type[64];
    int      content_length;
    char     body[HTTP_MAX_BODY];
    int      body_len;
    int      error;
} http_response_t;

int  http_get(const char* host, const char* path, http_response_t* resp);
void http_init(void);

#endif
