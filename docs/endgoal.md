## END RESULT

The following should be possible when done:

int main(void)
{
    os_init();
     
    os_task_create(led_blink_task, &btn_stack, PRIORITY_HIGH);
    os_task_create(button_handler_task, &btn_stack, PRIORITY_HIGH);
    os_task_create(uart_logger_task, &uart_stack, PRIORITY_MID);
    
    os_start();
}



What it must have:
1. Task/thread management- create,delete, suspend, resume tasks with seperate stacks.
2. Preemptive priority-based scheduler- higher priority task interrupts lower, using SysTick or PendSV.
3. Context switching- Saving/restoring registers.
4. Synchronization primitives- at minimum binary semaphore and mutex with priority inheritance. 
5. Timing services- delay functions, basic tick counting. 
6. Idle task- runs when nothing else can.


Good adds:
Memory allocator
Message queues
Sheell over UART
Stack overflow detection

