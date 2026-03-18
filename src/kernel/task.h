#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS       16
#define TASK_STACK_SIZE  8192
#define TASK_NAME_LEN   32

/* Gorev durumu */
#define TASK_STATE_FREE     0
#define TASK_STATE_READY    1
#define TASK_STATE_RUNNING  2
#define TASK_STATE_SLEEPING 3
#define TASK_STATE_BLOCKED  4
#define TASK_STATE_DEAD     5

/* Gorev onceligi */
#define TASK_PRIORITY_LOW    0
#define TASK_PRIORITY_NORMAL 1
#define TASK_PRIORITY_HIGH   2
#define TASK_PRIORITY_SYSTEM 3

typedef void (*task_func_t)(void);

typedef struct {
    uint32_t esp;                     /* Stack pointer */
    uint32_t ebp;
    uint32_t eip;
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi;
} task_regs_t;

typedef struct {
    char         name[TASK_NAME_LEN];
    int          state;
    int          priority;
    int          id;
    task_func_t  entry;
    task_regs_t  regs;
    uint8_t      stack[TASK_STACK_SIZE];
    uint32_t     cpu_time;            /* Toplam calisma suresi (tick) */
    uint32_t     sleep_until;         /* Uyuma zamani */
    uint32_t     mem_usage;           /* Tahmini bellek kullanimi */
} task_t;

/* Fonksiyonlar */
void     task_init(void);
int      task_create(const char* name, task_func_t func, int priority);
void     task_yield(void);
void     task_sleep(uint32_t ms);
void     task_exit(void);
void     task_kill(int id);
void     task_schedule(void);

/* Bilgi */
int      task_get_count(void);
task_t*  task_get(int index);
task_t*  task_get_current(void);
int      task_get_current_id(void);
uint32_t task_get_ticks(void);

#endif
