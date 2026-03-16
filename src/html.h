#ifndef HTML_H
#define HTML_H

#include <stdint.h>

#define HTML_MAX_NODES 128

/* Render komutu tipleri */
#define HTML_TEXT     0
#define HTML_HEADING  1
#define HTML_BOLD     2
#define HTML_ITALIC   3
#define HTML_LINK     4
#define HTML_HR       5
#define HTML_NEWLINE  6
#define HTML_LIST     7

typedef struct {
    int     type;
    int     level;      /* heading: 1-6 */
    char    text[128];
    char    href[128];  /* link URL */
    uint32_t color;
} html_node_t;

typedef struct {
    html_node_t nodes[HTML_MAX_NODES];
    int         count;
    char        title[128];
} html_doc_t;

void html_parse(const char* html, int len, html_doc_t* doc);

#endif
