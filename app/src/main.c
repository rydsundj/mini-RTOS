#include "os_functions.h"

#define LED_STACK_SIZE  256
#define BTN_STACK_SIZE  256

static uint32_t led_stack[LED_STACK_SIZE];
static uint32_t btn_stack[BTN_STACK_SIZE];

static void led_blink_task(void)
{
    for (;;) {
        os_task_delay(500);
    }
}

static void button_handler_task(void)
{
    for (;;) {
        os_task_delay(50);
    }
}

int main(void)
{
    os_init();

    os_task_create(led_blink_task,     led_stack, LED_STACK_SIZE, PRIORITY_HIGH);
    os_task_create(button_handler_task, btn_stack, BTN_STACK_SIZE, PRIORITY_HIGH);

    os_start();

    for (;;);
}
