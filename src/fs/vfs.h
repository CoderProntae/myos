#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#define VFS_MAX_FILES    32
#define VFS_MAX_NAME     32
#define VFS_MAX_PATH     128

#define VFS_FILE         0
#define VFS_DIRECTORY    1

typedef struct {
    char     name[VFS_MAX_NAME];
    uint8_t  type;          /* VFS_FILE veya VFS_DIRECTORY */
    int      parent;        /* ust klasor indeksi, -1=root */
    uint32_t size;
    uint8_t* data;          /* dosya icerigi (heap'te) */
    int      active;
} vfs_node_t;

/* Fonksiyonlar */
void  vfs_init(void);
int   vfs_create_dir(const char* name, int parent);
int   vfs_create_file(const char* name, int parent, const void* data, uint32_t size);
int   vfs_write_file(int node, const void* data, uint32_t size);
int   vfs_append_file(int node, const void* data, uint32_t size);
int   vfs_read_file(int node, void* buffer, uint32_t max_size);
int   vfs_find(const char* name, int parent);
int   vfs_find_path(const char* path);
int   vfs_delete(int node);

int         vfs_get_node_count(void);
vfs_node_t* vfs_get_node(int index);
int         vfs_count_children(int parent);
int         vfs_get_child(int parent, int n);
void        vfs_build_path(int node, char* path);

#endif
