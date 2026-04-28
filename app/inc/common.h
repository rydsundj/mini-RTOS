#pragma once

#include <stdint.h>

#define MAX_TASKS       16
#define IDLE_STACK_SIZE 128

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
} task_state_t;

typedef enum {
    PRIORITY_LOW  = 0,
    PRIORITY_MID  = 1,
    PRIORITY_HIGH = 2,
} task_priority_t;

typedef struct task {
    uint32_t       *sp;
    uint32_t       *stack_base;
    uint32_t        stack_size;
    task_state_t    state;
    task_priority_t priority;
    uint32_t        delay_ticks;
    struct task    *next;
} task_t;

typedef enum {
    OS_NOT_RUNNING,
    OS_RUNNING,
} os_state_t;

typedef struct {
    os_state_t  state;
    task_t     *current;
    task_t     *ready[3];
    uint32_t    tick_count;
    uint32_t    task_count;
} os_kernel_t;
