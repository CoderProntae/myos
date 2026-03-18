#include "task.h"
#include "string.h"
#include "io.h"
#include "net.h"

static task_t tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = 0;
static uint32_t ticks = 0;
static int scheduler_active = 0;

/* ======== PIT Timer ======== */

#define PIT_FREQ 100   /* 100 Hz = 10ms per tick */

static void pit_init(void) {
    uint32_t divisor = 1193180 / PIT_FREQ;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

/* ======== Init ======== */

void task_init(void) {
    k_memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    current_task = 0;
    ticks = 0;

    /* Task 0: Kernel (ana gorev — zaten calisiyor) */
    task_t* t = &tasks[0];
    k_strcpy(t->name, "kernel");
    t->state = TASK_STATE_RUNNING;
    t->priority = TASK_PRIORITY_SYSTEM;
    t->id = 0;
    t->entry = 0;
    t->cpu_time = 0;
    t->mem_usage = 0;
    task_count = 1;

    pit_init();
    scheduler_active = 1;
}

/* ======== Task Olustur ======== */

int task_create(const char* name, task_func_t func, int priority) {
    /* Bos slot bul */
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_FREE ||
            tasks[i].state == TASK_STATE_DEAD) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    task_t* t = &tasks[slot];
    k_memset(t, 0, sizeof(task_t));

    k_strcpy(t->name, name);
    t->state = TASK_STATE_READY;
    t->priority = priority;
    t->id = slot;
    t->entry = func;
    t->cpu_time = 0;
    t->mem_usage = TASK_STACK_SIZE;

    /* Stack hazirla — stack yukari dogru buyur */
    uint32_t stack_top = (uint32_t)&t->stack[TASK_STACK_SIZE - 4];
    /* Stack'e donus adresi olarak task_exit koy */
    stack_top -= 4;
    *(uint32_t*)stack_top = (uint32_t)task_exit;
    /* Entry point */
    t->regs.esp = stack_top;
    t->regs.ebp = stack_top;
    t->regs.eip = (uint32_t)func;

    task_count++;
    return slot;
}

/* ======== Cooperative Yield ======== */

void task_yield(void) {
    if (!scheduler_active) return;

    /* Tick artir */
    ticks++;
    tasks[current_task].cpu_time++;

    /* Sonraki hazir gorevi bul */
    int next = current_task;
    for (int tries = 0; tries < MAX_TASKS; tries++) {
        next = (next + 1) % MAX_TASKS;

        /* Uyuyan gorevleri kontrol et */
        if (tasks[next].state == TASK_STATE_SLEEPING) {
            if (ticks >= tasks[next].sleep_until) {
                tasks[next].state = TASK_STATE_READY;
            }
        }

        if (tasks[next].state == TASK_STATE_READY) {
            /* Gorev degistir */
            if (next != current_task) {
                int old = current_task;

                if (tasks[old].state == TASK_STATE_RUNNING)
                    tasks[old].state = TASK_STATE_READY;

                tasks[next].state = TASK_STATE_RUNNING;
                current_task = next;

                /* Basit context switch: kayitlari kaydet/yukle */
                /* Not: Gercek context switch assembly gerektirir */
                /* Cooperative modelde gorev kendisi yield ettiginden */
                /* sadece fonksiyon cagirarak gecis yapiyoruz */

                /* Eger yeni gorev hic baslatilmamissa entry'yi cagir */
                if (tasks[next].cpu_time == 0 && tasks[next].entry) {
                    tasks[next].cpu_time = 1;
                    tasks[next].entry();
                    /* Gorev bitti */
                    tasks[next].state = TASK_STATE_DEAD;
                    current_task = old;
                    tasks[old].state = TASK_STATE_RUNNING;
                }
            }
            return;
        }
    }
}

/* ======== Sleep ======== */

void task_sleep(uint32_t ms) {
    uint32_t sleep_ticks = ms / (1000 / PIT_FREQ);
    if (sleep_ticks == 0) sleep_ticks = 1;

    tasks[current_task].state = TASK_STATE_SLEEPING;
    tasks[current_task].sleep_until = ticks + sleep_ticks;

    task_yield();
}

/* ======== Exit ======== */

void task_exit(void) {
    tasks[current_task].state = TASK_STATE_DEAD;
    task_count--;
    task_yield();
    /* Buraya donmemeli — sonsuz dongu */
    while (1);
}

/* ======== Kill ======== */

void task_kill(int id) {
    if (id <= 0 || id >= MAX_TASKS) return;  /* Kernel oldurulemez */
    if (tasks[id].state == TASK_STATE_FREE) return;
    tasks[id].state = TASK_STATE_DEAD;
    task_count--;
}

/* ======== Schedule (ana donguden cagrilir) ======== */

void task_schedule(void) {
    if (!scheduler_active) return;
    ticks++;
    tasks[current_task].cpu_time++;

    /* Uyuyan gorevleri uyandır */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_STATE_SLEEPING && ticks >= tasks[i].sleep_until)
            tasks[i].state = TASK_STATE_READY;
    }
}

/* ======== Bilgi ======== */

int task_get_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_STATE_FREE &&
            tasks[i].state != TASK_STATE_DEAD)
            count++;
    }
    return count;
}

task_t* task_get(int index) {
    if (index >= 0 && index < MAX_TASKS)
        return &tasks[index];
    return 0;
}

task_t* task_get_current(void) {
    return &tasks[current_task];
}

int task_get_current_id(void) {
    return current_task;
}

uint32_t task_get_ticks(void) {
    return ticks;
}
