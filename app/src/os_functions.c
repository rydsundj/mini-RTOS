#include "os_functions.h"
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <string.h>

os_kernel_t kernel;

static task_t  task_pool[MAX_TASKS];
static uint32_t idle_stack[IDLE_STACK_SIZE];
static uint32_t task_pool_idx = 0;

/* -- internal helpers ---------------------------------------------------- */

static uint32_t *stack_init(uint32_t *stack_top, void (*task_func)(void))
{
    /* Cortex-M exception frame: xPSR, PC, LR, R12, R3, R2, R1, R0
       plus software-saved R4-R11. Stack grows down. */
    stack_top--;
    *stack_top-- = 0x01000000;          /* xPSR: thumb bit */
    *stack_top-- = (uint32_t)task_func; /* PC */
    *stack_top-- = 0xFFFFFFFD;          /* LR: return to thread mode, use PSP */
    *stack_top-- = 0;                   /* R12 */
    *stack_top-- = 0;                   /* R3 */
    *stack_top-- = 0;                   /* R2 */
    *stack_top-- = 0;                   /* R1 */
    *stack_top-- = 0;                   /* R0 */
    /* software context: R11-R4 */
    *stack_top-- = 0xBBBBBBBB;         /* R11 (debug sentinel) */
    *stack_top-- = 0xAAAAAAAA;         /* R10 */
    *stack_top-- = 0x99999999;         /* R9 */
    *stack_top-- = 0x88888888;         /* R8 */
    *stack_top-- = 0x77777777;         /* R7 */
    *stack_top-- = 0x66666666;         /* R6 */
    *stack_top-- = 0x55555555;         /* R5 */
    *stack_top   = 0x44444444;         /* R4 */
    return stack_top;
}

static void ready_push(task_t *task)
{
    task_priority_t p = task->priority;
    task->next = kernel.ready[p];
    kernel.ready[p] = task;
}

static task_t *pick_next(void)
{
    for (int p = PRIORITY_HIGH; p >= PRIORITY_LOW; p--) {
        if (kernel.ready[p]) {
            task_t *t = kernel.ready[p];
            kernel.ready[p] = t->next;
            t->next = NULL;
            return t;
        }
    }
    return NULL;
}

static void idle_task(void)
{
    for (;;) {
        __asm__ volatile("wfi");
    }
}

/* -- public API ---------------------------------------------------------- */

void os_init(void)
{
    memset(&kernel, 0, sizeof(kernel));
    kernel.state = OS_NOT_RUNNING;

    /* idle task always exists at lowest priority */
    os_task_create(idle_task, idle_stack, IDLE_STACK_SIZE, PRIORITY_LOW);
}

void os_task_create(void (*task_func)(void), uint32_t *stack, uint32_t stack_size, task_priority_t priority)
{
    if (task_pool_idx >= MAX_TASKS)
        return;

    task_t *t = &task_pool[task_pool_idx++];
    t->stack_base  = stack;
    t->stack_size  = stack_size * sizeof(uint32_t);
    t->priority    = priority;
    t->state       = TASK_READY;
    t->delay_ticks = 0;
    t->next        = NULL;

    uint32_t *top = stack + stack_size;
    t->sp = stack_init(top, task_func);

    ready_push(t);
    kernel.task_count++;
}

void os_start(void)
{
    /* SysTick at 1 ms assuming 16 MHz HSI */
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(16000 - 1);
    systick_interrupt_enable();
    systick_counter_enable();

    kernel.state = OS_RUNNING;
    kernel.current = pick_next();
    kernel.current->state = TASK_RUNNING;

    /* load first task's PSP and drop to thread mode */
    __asm__ volatile(
        "msr psp, %0          \n"
        "mov r0, #2           \n"   /* CONTROL: use PSP, unprivileged */
        "msr control, r0      \n"
        "isb                  \n"
        "pop {r4-r11}         \n"   /* software context */
        "pop {r0-r3, r12, lr} \n"   /* hw context minus PC/xPSR */
        "pop {pc}             \n"   /* jump to task */
        :: "r"(kernel.current->sp)
    );
}

void os_task_delay(uint32_t ticks)
{
    kernel.current->delay_ticks = ticks;
    kernel.current->state = TASK_BLOCKED;
    os_yield();
}

void os_task_suspend(task_t *task)
{
    task->state = TASK_SUSPENDED;
    if (task == kernel.current)
        os_yield();
}

void os_task_resume(task_t *task)
{
    if (task->state == TASK_SUSPENDED) {
        task->state = TASK_READY;
        ready_push(task);
    }
}

void os_yield(void)
{
    /* trigger PendSV to do the context switch */
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

/* -- SysTick ISR --------------------------------------------------------- */

void sys_tick_handler(void)
{
    kernel.tick_count++;

    /* wake tasks whose delay has expired */
    for (uint32_t i = 0; i < task_pool_idx; i++) {
        task_t *t = &task_pool[i];
        if (t->state == TASK_BLOCKED && t->delay_ticks > 0) {
            t->delay_ticks--;
            if (t->delay_ticks == 0) {
                t->state = TASK_READY;
                ready_push(t);
            }
        }
    }

    /* preempt if a higher-priority task became ready */
    task_t *next = pick_next();
    if (next) {
        if (kernel.current->state == TASK_RUNNING)
            kernel.current->state = TASK_READY;
        /* put current back if still runnable */
        if (kernel.current->state == TASK_READY)
            ready_push(kernel.current);
        kernel.current = next;
        SCB_ICSR |= SCB_ICSR_PENDSVSET;
    }
}


__attribute__((naked)) void pend_sv_handler(void)
{
    __asm__ volatile(
        /* save current task's software context */
        "mrs   r0, psp          \n"
        "stmdb r0!, {r4-r11}    \n"
        "ldr   r1, =kernel      \n"
        "ldr   r2, [r1, %[cur]] \n"   /* kernel.current */
        "str   r0, [r2]         \n"   /* task->sp = r0 */

        /* pick next task */
        "push  {r1, lr}         \n"
        "bl    pick_next_asm    \n"
        "pop   {r1, lr}         \n"

        /* r0 = new task (pick_next_asm returns it) */
        "str   r0, [r1, %[cur]] \n"   /* kernel.current = next */
        "ldr   r0, [r0]         \n"   /* r0 = next->sp */

        /* restore next task's software context */
        "ldmia r0!, {r4-r11}    \n"
        "msr   psp, r0          \n"
        "orr   lr, lr, #0x04    \n"   /* return to thread/PSP */
        "bx    lr               \n"
        ::
        [cur] "i"(__builtin_offsetof(os_kernel_t, current))
    );
}

/* small C trampoline so PendSV can call the scheduler */
task_t *pick_next_asm(void)
{
    if (kernel.current && kernel.current->state == TASK_RUNNING) {
        kernel.current->state = TASK_READY;
        ready_push(kernel.current);
    }
    task_t *next = pick_next();
    next->state = TASK_RUNNING;
    return next;
}
