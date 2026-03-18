#include "elf.h"
#include "heap.h"
#include "string.h"
#include "vfs.h"

/* Program yukleme alani — 1MB */
#define PROG_AREA_SIZE (1024 * 1024)
static uint8_t prog_area[PROG_AREA_SIZE] __attribute__((aligned(4096)));

static int programs_run = 0;
static int programs_failed = 0;

void elf_init(void) {
    k_memset(prog_area, 0, PROG_AREA_SIZE);
    programs_run = 0;
    programs_failed = 0;
}

/* ======== ELF Dogrulama ======== */

int elf_validate(const void* data, uint32_t size) {
    if (!data || size < sizeof(elf32_header_t))
        return -1;

    const elf32_header_t* hdr = (const elf32_header_t*)data;

    /* Magic number kontrol */
    uint32_t magic = *(uint32_t*)hdr->e_ident;
    if (magic != ELF_MAGIC)
        return -2;  /* ELF degil */

    /* 32-bit mi? */
    if (hdr->e_ident[4] != 1)
        return -3;  /* 64-bit — desteklenmiyor */

    /* Little endian mi? */
    if (hdr->e_ident[5] != 1)
        return -4;  /* Big endian */

    /* i386 mi? */
    if (hdr->e_machine != EM_386)
        return -5;  /* Yanlis mimari */

    /* Executable mi? */
    if (hdr->e_type != ET_EXEC)
        return -6;  /* Executable degil */

    /* Entry point var mi? */
    if (hdr->e_entry == 0)
        return -7;  /* Entry point yok */

    /* Program header var mi? */
    if (hdr->e_phnum == 0)
        return -8;  /* Segment yok */

    return 0;  /* Gecerli ELF */
}

/* ======== ELF Yukle ve Calistir ======== */

int elf_load_and_run(const void* data, uint32_t size) {
    int valid = elf_validate(data, size);
    if (valid != 0) {
        programs_failed++;
        return valid;
    }

    const elf32_header_t* hdr = (const elf32_header_t*)data;
    const uint8_t* file_data = (const uint8_t*)data;

    /* Program header'lari isle */
    for (uint16_t i = 0; i < hdr->e_phnum; i++) {
        uint32_t ph_offset = hdr->e_phoff + i * hdr->e_phentsize;
        if (ph_offset + sizeof(elf32_phdr_t) > size)
            continue;

        const elf32_phdr_t* phdr = (const elf32_phdr_t*)(file_data + ph_offset);

        /* Sadece LOAD segmentlerini isle */
        if (phdr->p_type != PT_LOAD)
            continue;

        /* Boyut kontrol */
        if (phdr->p_memsz > PROG_AREA_SIZE) {
            programs_failed++;
            return -10;  /* Program cok buyuk */
        }

        /* Dosyadan segment'i yukle */
        if (phdr->p_offset + phdr->p_filesz <= size) {
            /* Hedef adresi prog_area'ya yonlendir */
            uint32_t load_offset = phdr->p_vaddr & 0xFFFFF;  /* Alt 20 bit */
            if (load_offset + phdr->p_memsz > PROG_AREA_SIZE)
                continue;

            /* Once sifirla (BSS icin) */
            k_memset(prog_area + load_offset, 0, phdr->p_memsz);

            /* Dosyadan kopyala */
            const uint8_t* src = file_data + phdr->p_offset;
            for (uint32_t j = 0; j < phdr->p_filesz; j++)
                prog_area[load_offset + j] = src[j];
        }
    }

    /* Entry point'e atla */
    uint32_t entry_offset = hdr->e_entry & 0xFFFFF;
    if (entry_offset >= PROG_AREA_SIZE) {
        programs_failed++;
        return -11;
    }

    program_entry_t entry = (program_entry_t)(prog_area + entry_offset);

    programs_run++;
    int result = entry();

    return result;
}

/* ======== VFS'ten Yukle ======== */

int elf_load_from_vfs(const char* path) {
    int node = vfs_find_path(path);
    if (node < 0) return -20;

    vfs_node_t* n = vfs_get_node(node);
    if (!n || n->type != VFS_FILE) return -21;
    if (!n->data || n->size == 0) return -22;

    /* ELF mi MYOS mi kontrol et */
    if (n->size >= 4) {
        uint32_t magic = *(uint32_t*)n->data;
        if (magic == ELF_MAGIC)
            return elf_load_and_run(n->data, n->size);
        if (magic == MYOS_PROG_MAGIC)
            return myos_load_and_run(n->data, n->size);
    }

    return -23;  /* Bilinmeyen format */
}

/* ======== MyOS Basit Program Formati ======== */

int myos_load_and_run(const void* data, uint32_t size) {
    if (size < sizeof(myos_program_t))
        return -30;

    const myos_program_t* prog = (const myos_program_t*)data;

    if (prog->magic != MYOS_PROG_MAGIC)
        return -31;

    if (prog->entry_offset + prog->code_size > size)
        return -32;

    if (prog->code_size > PROG_AREA_SIZE)
        return -33;

    /* Kodu prog_area'ya kopyala */
    const uint8_t* code = (const uint8_t*)data + prog->entry_offset;
    for (uint32_t i = 0; i < prog->code_size; i++)
        prog_area[i] = code[i];

    /* Calistir */
    program_entry_t entry = (program_entry_t)prog_area;
    programs_run++;
    return entry();
}
