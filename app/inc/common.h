
enum os_state
{
    OS_NOT_RUNNING,
    OS_RUNNING
};

struct os_kernel
{
    enum os_state state;
    struct task *current;
};  