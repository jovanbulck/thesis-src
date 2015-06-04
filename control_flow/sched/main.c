#include "common.h"
#include "foobar.h"
#include "scheduler.h"

int main()
{
    WDTCTL = WDTPW | WDTHOLD;
    uart_init();
    printf_int("\n---------------\n[main] started, I have id %d\n\n",
    sancus_get_self_id());
    
    sancus_enable(&a);
    sancus_enable(&foo);
    sancus_enable(&bar);
    sancus_enable(&scheduler);

    //do_control_flow_integrity_hack();
    
    puts("[main] set sm internal vars");
    set_a_vars();
    set_bar_vars();
    set_foo_vars();
    
    puts("[main] initializing scheduler");
    initialize_scheduler();
    
    register_thread_portal(&a, SM_GET_ENTRY_IDX(a, enter_a));
    register_thread_portal(&foo, SM_GET_ENTRY_IDX(foo, enter_foo));    
    register_thread_portal(&bar, SM_GET_ENTRY_IDX(bar, call_a_illegal_ret));
    dump_scheduler();
    
    puts("[main] dumping thr ids");
    print_a_cur_thr_id();
    print_bar_cur_thr_id();
    print_foo_cur_thr_id();
    
    puts("[main] starting scheduler");
    start_scheduling();

    puts("[main] return from scheduler:");
    dump_scheduler();
    
    puts("[main] dumping thr ids");
    print_a_cur_thr_id();
    print_bar_cur_thr_id();
    print_foo_cur_thr_id();
    
    puts("\n[main] exiting\n-----------------");
    EXIT
}

int putchar(int c)
{
    if (c == '\n')
        putchar('\r');

    uart_write_byte(c);
    return c;
}

void __attribute__((interrupt(SM_VECTOR))) the_isr(void) {
    printerr("\nVIOLATION HAS BEEN DETECTED: stopping the system...");
    EXIT
}
