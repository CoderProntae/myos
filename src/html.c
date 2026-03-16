#include "html.h"
#include "string.h"

static void add_node(html_doc_t* doc, int type, int level,
                     const char* text, uint32_t color) {
    if (doc->count >= HTML_MAX_NODES) return;
    html_node_t* n = &doc->nodes[doc->count++];
    n->type = type;
    n->level = level;
    n->color = color;
    n->href[0] = '\0';
    if (text) {
        int i = 0;
        while (text[i] && i < 127) { n->text[i] = text[i]; i++; }
        n->text[i] = '\0';
    } else {
        n->text[0] = '\0';
    }
}

/* Tag ismini parse et: "<tag" -> "tag" */
static int get_tag(const char* s, int len, char* tag, int max) {
    int i = 0;
    if (i < len && s[i] == '<') i++;
    if (i < len && s[i] == '/') i++;  /* closing tag */
    int j = 0;
    while (i < len && s[i] != '>' && s[i] != ' ' && s[i] != '/' && j < max - 1) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c += 32;  /* lowercase */
        tag[j++] = c;
        i++;
    }
    tag[j] = '\0';
    return j;
}

/* Closing tag mi? */
static int is_closing(const char* s) {
    return (s[0] == '<' && s[1] == '/');
}

/* Tag icinden attribute bul: href="..." */
static void get_attr(const char* tag_start, int tag_len,
                     const char* attr, char* out, int max) {
    out[0] = '\0';
    int alen = k_strlen(attr);
    for (int i = 0; i < tag_len - alen; i++) {
        int match = 1;
        for (int j = 0; j < alen; j++) {
            char a = tag_start[i + j];
            char b = attr[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = 0; break; }
        }
        if (match) {
            int start = i + alen;
            /* = ve " atla */
            while (start < tag_len && (tag_start[start] == '=' || tag_start[start] == '"' || tag_start[start] == '\''))
                start++;
            int end = start;
            while (end < tag_len && tag_start[end] != '"' && tag_start[end] != '\'' && tag_start[end] != '>')
                end++;
            int copy = end - start;
            if (copy >= max) copy = max - 1;
            for (int j = 0; j < copy; j++) out[j] = tag_start[start + j];
            out[copy] = '\0';
            return;
        }
    }
}

/* Bosluk ve \n temizle */
static void trim(char* s) {
    /* Bas */
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r') start++;
    if (start > 0) {
        int i = 0;
        while (s[start + i]) { s[i] = s[start + i]; i++; }
        s[i] = '\0';
    }
    /* Son */
    int len = k_strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n' || s[len-1] == '\r')) {
        s[--len] = '\0';
    }
}

/* ======== Ana Parser ======== */

void html_parse(const char* html, int len, html_doc_t* doc) {
    k_memset(doc, 0, sizeof(html_doc_t));

    int in_bold = 0;
    int in_italic = 0;
    int in_link = 0;
    int in_title = 0;
    int in_li = 0;
    int heading_level = 0;
    char link_href[128] = {0};

    char text_buf[256];
    int text_pos = 0;

    int i = 0;
    while (i < len && doc->count < HTML_MAX_NODES) {
        if (html[i] == '<') {
            /* Onceki metni flush et */
            if (text_pos > 0) {
                text_buf[text_pos] = '\0';
                trim(text_buf);
                if (k_strlen(text_buf) > 0) {
                    if (in_title) {
                        k_strcpy(doc->title, text_buf);
                    } else if (heading_level > 0) {
                        add_node(doc, HTML_HEADING, heading_level, text_buf, 0x55CCFF);
                    } else if (in_link) {
                        html_node_t* n = &doc->nodes[doc->count];
                        add_node(doc, HTML_LINK, 0, text_buf, 0x5599FF);
                        k_strcpy(n->href, link_href);
                    } else if (in_bold) {
                        add_node(doc, HTML_BOLD, 0, text_buf, 0xFFFFFF);
                    } else if (in_italic) {
                        add_node(doc, HTML_ITALIC, 0, text_buf, 0xCCCCDD);
                    } else if (in_li) {
                        add_node(doc, HTML_LIST, 0, text_buf, 0xCCCCCC);
                    } else {
                        add_node(doc, HTML_TEXT, 0, text_buf, 0xCCCCCC);
                    }
                }
                text_pos = 0;
            }

            /* Tag bul */
            int tag_start = i;
            int tag_end = i;
            while (tag_end < len && html[tag_end] != '>') tag_end++;
            if (tag_end < len) tag_end++;

            int tag_len = tag_end - tag_start;
            char tag[16];
            get_tag(html + tag_start, tag_len, tag, 16);
            int closing = is_closing(html + tag_start);

            /* Tag islemleri */
            if (!k_strcmp(tag, "h1")) {
                if (closing) { heading_level = 0; add_node(doc, HTML_NEWLINE, 0, 0, 0); }
                else heading_level = 1;
            }
            else if (!k_strcmp(tag, "h2")) {
                if (closing) { heading_level = 0; add_node(doc, HTML_NEWLINE, 0, 0, 0); }
                else heading_level = 2;
            }
            else if (!k_strcmp(tag, "h3")) {
                if (closing) { heading_level = 0; add_node(doc, HTML_NEWLINE, 0, 0, 0); }
                else heading_level = 3;
            }
            else if (!k_strcmp(tag, "b") || !k_strcmp(tag, "strong")) {
                in_bold = closing ? 0 : 1;
            }
            else if (!k_strcmp(tag, "i") || !k_strcmp(tag, "em")) {
                in_italic = closing ? 0 : 1;
            }
            else if (!k_strcmp(tag, "a")) {
                if (closing) { in_link = 0; }
                else {
                    in_link = 1;
                    get_attr(html + tag_start, tag_len, "href", link_href, 128);
                }
            }
            else if (!k_strcmp(tag, "p")) {
                if (closing) add_node(doc, HTML_NEWLINE, 0, 0, 0);
            }
            else if (!k_strcmp(tag, "br")) {
                add_node(doc, HTML_NEWLINE, 0, 0, 0);
            }
            else if (!k_strcmp(tag, "hr")) {
                add_node(doc, HTML_HR, 0, 0, 0x444466);
            }
            else if (!k_strcmp(tag, "li")) {
                if (closing) { in_li = 0; add_node(doc, HTML_NEWLINE, 0, 0, 0); }
                else in_li = 1;
            }
            else if (!k_strcmp(tag, "title")) {
                in_title = closing ? 0 : 1;
            }
            else if (!k_strcmp(tag, "div") || !k_strcmp(tag, "section")) {
                if (closing) add_node(doc, HTML_NEWLINE, 0, 0, 0);
            }

            i = tag_end;
        }
        else if (html[i] == '&') {
            /* HTML entity */
            if (i + 3 < len && html[i+1]=='l' && html[i+2]=='t' && html[i+3]==';') {
                if (text_pos < 255) text_buf[text_pos++] = '<';
                i += 4;
            } else if (i + 3 < len && html[i+1]=='g' && html[i+2]=='t' && html[i+3]==';') {
                if (text_pos < 255) text_buf[text_pos++] = '>';
                i += 4;
            } else if (i + 4 < len && html[i+1]=='a' && html[i+2]=='m' && html[i+3]=='p' && html[i+4]==';') {
                if (text_pos < 255) text_buf[text_pos++] = '&';
                i += 5;
            } else {
                if (text_pos < 255) text_buf[text_pos++] = '&';
                i++;
            }
        }
        else {
            /* Normal karakter */
            char c = html[i];
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            /* Ardisik bosluklari birlestir */
            if (c == ' ' && text_pos > 0 && text_buf[text_pos - 1] == ' ') {
                i++;
                continue;
            }
            if (text_pos < 255) text_buf[text_pos++] = c;
            i++;
        }
    }

    /* Kalan metni flush */
    if (text_pos > 0) {
        text_buf[text_pos] = '\0';
        trim(text_buf);
        if (k_strlen(text_buf) > 0) {
            add_node(doc, HTML_TEXT, 0, text_buf, 0xCCCCCC);
        }
    }
}
