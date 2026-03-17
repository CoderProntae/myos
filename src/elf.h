#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* ELF32 Header */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;        /* Programin baslangic adresi */
    uint32_t e_phoff;        /* Program header tablosu offset */
    uint32_t e_shoff;        /* Section header tablosu offset */
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;        /* Program header sayisi */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_header_t;

/* Program Header (segment) */
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;       /* Dosyadaki offset */
    uint32_t p_vaddr;        /* Yuklenecek sanal adres */
    uint32_t p_paddr;
    uint32_t p_filesz;       /* Dosyadaki boyut */
    uint32_t p_memsz;        /* Bellekteki boyut */
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

/* ELF sabitleri */
#define ELF_MAGIC       0x464C457F  /* "\x7FELF" */
#define ET_EXEC         2           /* Executable */
#define EM_386          3           /* Intel 80386 */
#define PT_LOAD         1           /* Yuklenecek segment */

/* PF flags */
#define PF_X            0x1         /* Execute */
#define PF_W            0x2         /* Write */
#define PF_R            0x4         /* Read */

/* Program cikis kodu */
typedef int (*program_entry_t)(void);

/* Fonksiyonlar */
void elf_init(void);
int  elf_validate(const void* data, uint32_t size);
int  elf_load_and_run(const void* data, uint32_t size);
int  elf_load_from_vfs(const char* path);

/* Basit program formati (ELF olmadan) */
typedef struct {
    uint32_t magic;          /* 0x4D594F53 = "MYOS" */
    uint32_t entry_offset;   /* Kodun baslangic offseti */
    uint32_t code_size;
    uint32_t version;
} __attribute__((packed)) myos_program_t;

#define MYOS_PROG_MAGIC 0x4D594F53

int myos_load_and_run(const void* data, uint32_t size);

#endif
