#include "vfs.h"
#include "heap.h"
#include "string.h"

static vfs_node_t nodes[VFS_MAX_FILES];
static int node_count = 0;

/* ======== Init ======== */

void vfs_init(void) {
    k_memset(nodes, 0, sizeof(nodes));
    node_count = 0;

    /* Varsayilan klasorler */
    vfs_create_dir("home", -1);
    vfs_create_dir("sys", -1);
    vfs_create_dir("bin", -1);
    vfs_create_dir("tmp", -1);
    vfs_create_dir("docs", 0);

    /* Varsayilan dosyalar */
    const char* readme =
        "MyOS v0.3\n"
        "=========\n\n"
        "Sifirdan yazilmis isletim sistemi.\n"
        "C ve Assembly ile gelistirildi.\n\n"
        "Ozellikler:\n"
        "- VESA grafik (800x600x32)\n"
        "- PS/2 mouse ve klavye\n"
        "- PCI bus tarama\n"
        "- E1000 ag karti surucusu\n"
        "- TCP/IP ag yigini\n"
        "- HTTP tarayici\n"
        "- Dosya sistemi (VFS)\n"
        "- Heap bellek yonetimi\n";
    vfs_create_file("readme.txt", 0, readme, k_strlen(readme));

    const char* notes =
        "Yapilacaklar:\n"
        "- ELF program yukleme\n"
        "- Multitasking\n"
        "- libc port\n"
        "- HTTPS destegi\n";
    vfs_create_file("notlar.txt", 4, notes, k_strlen(notes));

    const char* sysinfo =
        "[Sistem]\n"
        "kernel=myos\n"
        "version=0.3.0\n"
        "arch=i686\n"
        "video=vesa\n"
        "net=e1000\n";
    vfs_create_file("config.sys", 1, sysinfo, k_strlen(sysinfo));

    const char* hello =
        "Merhaba Dunya!\n"
        "Bu dosya VFS uzerinde saklanmaktadir.\n"
        "kmalloc ile heap bellekte tutulur.\n";
    vfs_create_file("merhaba.txt", 0, hello, k_strlen(hello));

    const char* test_html =
        "<!DOCTYPE html>\n"
        "<html><head><title>Test Sayfasi</title></head>\n"
        "<body>\n"
        "<h1>Yerel HTML Sayfasi</h1>\n"
        "<p>Bu sayfa <b>VFS</b> dosya sisteminden yuklenmistir.</p>\n"
        "<hr>\n"
        "<h2>Ozellikler</h2>\n"
        "<ul>\n"
        "<li>Basliklar (h1, h2, h3)</li>\n"
        "<li><b>Kalin</b> metin</li>\n"
        "<li><a href='#'>Link rengi</a></li>\n"
        "<li>Listeler</li>\n"
        "</ul>\n"
        "<p>MyOS Browser ile goruntuleniyorsunuz.</p>\n"
        "</body></html>\n";
    vfs_create_file("index.html", 0, test_html, k_strlen(test_html));
}

/* ======== Klasor Olustur ======== */

int vfs_create_dir(const char* name, int parent) {
    if (node_count >= VFS_MAX_FILES) return -1;
    int idx = node_count++;
    vfs_node_t* n = &nodes[idx];
    k_memset(n, 0, sizeof(vfs_node_t));
    k_strcpy(n->name, name);
    n->type = VFS_DIRECTORY;
    n->parent = parent;
    n->size = 0;
    n->data = 0;
    n->active = 1;
    return idx;
}

/* ======== Dosya Olustur ======== */

int vfs_create_file(const char* name, int parent, const void* data, uint32_t size) {
    if (node_count >= VFS_MAX_FILES) return -1;
    int idx = node_count++;
    vfs_node_t* n = &nodes[idx];
    k_memset(n, 0, sizeof(vfs_node_t));
    k_strcpy(n->name, name);
    n->type = VFS_FILE;
    n->parent = parent;
    n->size = size;
    n->active = 1;

    if (data && size > 0) {
        n->data = (uint8_t*)kmalloc(size + 1);
        if (n->data) {
            const uint8_t* src = (const uint8_t*)data;
            for (uint32_t i = 0; i < size; i++)
                n->data[i] = src[i];
            n->data[size] = 0;
        }
    } else {
        n->data = 0;
    }
    return idx;
}

/* ======== Dosya Yaz ======== */

int vfs_write_file(int node, const void* data, uint32_t size) {
    if (node < 0 || node >= node_count) return -1;
    vfs_node_t* n = &nodes[node];
    if (!n->active || n->type != VFS_FILE) return -2;

    if (n->data) kfree(n->data);

    n->size = size;
    if (size > 0) {
        n->data = (uint8_t*)kmalloc(size + 1);
        if (!n->data) return -3;
        const uint8_t* src = (const uint8_t*)data;
        for (uint32_t i = 0; i < size; i++)
            n->data[i] = src[i];
        n->data[size] = 0;
    } else {
        n->data = 0;
    }
    return 0;
}

/* ======== Dosya Ekle (Append) ======== */

int vfs_append_file(int node, const void* data, uint32_t size) {
    if (node < 0 || node >= node_count) return -1;
    vfs_node_t* n = &nodes[node];
    if (!n->active || n->type != VFS_FILE) return -2;

    uint32_t new_size = n->size + size;
    uint8_t* new_data = (uint8_t*)kmalloc(new_size + 1);
    if (!new_data) return -3;

    if (n->data && n->size > 0) {
        for (uint32_t i = 0; i < n->size; i++)
            new_data[i] = n->data[i];
    }
    const uint8_t* src = (const uint8_t*)data;
    for (uint32_t i = 0; i < size; i++)
        new_data[n->size + i] = src[i];
    new_data[new_size] = 0;

    if (n->data) kfree(n->data);
    n->data = new_data;
    n->size = new_size;
    return 0;
}

/* ======== Dosya Oku ======== */

int vfs_read_file(int node, void* buffer, uint32_t max_size) {
    if (node < 0 || node >= node_count) return -1;
    vfs_node_t* n = &nodes[node];
    if (!n->active || n->type != VFS_FILE) return -2;
    if (!n->data) return 0;

    uint32_t copy = n->size > max_size ? max_size : n->size;
    uint8_t* dst = (uint8_t*)buffer;
    for (uint32_t i = 0; i < copy; i++)
        dst[i] = n->data[i];
    return (int)copy;
}

/* ======== Ara ======== */

int vfs_find(const char* name, int parent) {
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active &&
            nodes[i].parent == parent &&
            k_strcmp(nodes[i].name, name) == 0)
            return i;
    }
    return -1;
}

int vfs_find_path(const char* path) {
    if (!path || path[0] == '\0') return -1;

    const char* p = path;
    if (*p == '/') p++;

    int current = -1; /* root */

    while (*p) {
        char part[VFS_MAX_NAME];
        int pi = 0;
        while (*p && *p != '/' && pi < VFS_MAX_NAME - 1)
            part[pi++] = *p++;
        part[pi] = '\0';
        if (*p == '/') p++;
        if (pi == 0) continue;

        int found = vfs_find(part, current);
        if (found < 0) return -1;
        current = found;
    }
    return current;
}

/* ======== Sil ======== */

int vfs_delete(int node) {
    if (node < 0 || node >= node_count) return -1;
    vfs_node_t* n = &nodes[node];
    if (!n->active) return -2;

    /* Klasorse cocuklari kontrol et */
    if (n->type == VFS_DIRECTORY) {
        for (int i = 0; i < node_count; i++) {
            if (nodes[i].active && nodes[i].parent == node)
                return -3; /* Bos degil */
        }
    }

    if (n->data) {
        kfree(n->data);
        n->data = 0;
    }
    n->active = 0;
    return 0;
}

/* ======== Yardimci ======== */

int vfs_get_node_count(void) { return node_count; }

vfs_node_t* vfs_get_node(int index) {
    if (index >= 0 && index < node_count && nodes[index].active)
        return &nodes[index];
    return 0;
}

int vfs_count_children(int parent) {
    int c = 0;
    for (int i = 0; i < node_count; i++)
        if (nodes[i].active && nodes[i].parent == parent) c++;
    return c;
}

int vfs_get_child(int parent, int n) {
    int c = 0;
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && nodes[i].parent == parent) {
            if (c == n) return i;
            c++;
        }
    }
    return -1;
}

void vfs_build_path(int node, char* path) {
    if (node < 0) {
        k_strcpy(path, "/");
        return;
    }
    if (nodes[node].parent == -1) {
        path[0] = '/';
        k_strcpy(path + 1, nodes[node].name);
        return;
    }
    char tmp[VFS_MAX_PATH];
    vfs_build_path(nodes[node].parent, tmp);
    k_strcpy(path, tmp);
    int l = k_strlen(path);
    if (l > 1) { path[l] = '/'; path[l + 1] = '\0'; }
    k_strcpy(path + k_strlen(path), nodes[node].name);
}
