#pragma once

#include "common.h"

void os_init(void);
void os_start(void);
void os_task_create(void (*task_func)(void), uint32_t *stack, uint32_t stack_size, task_priority_t priority);
void os_task_delay(uint32_t ticks);
void os_task_suspend(task_t *task);
void os_task_resume(task_t *task);

void os_tick_handler(void);
void os_yield(void);

extern os_kernel_t kernel;
