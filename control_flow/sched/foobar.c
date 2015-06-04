#include "common.h"
#include "foobar.h"
#include "scheduler.h"

DECLARE_SM(a, 0x1234);
DECLARE_SM(foo, 0x1234);
DECLARE_SM(bar, 0x1234);

void SM_ENTRY("bar") enter_bar(void)
{
    printf_int("[bar::%d] enter_bar\n", __sm_bar_thr);
    printf_int("[bar::%d] yielding...\n", __sm_bar_thr);
    yield();
    printf_int("[bar::%d] returned from yield :-) Now dumping sched\n", __sm_bar_thr);
    printf_int("[bar::%d] dumping scheduler\n", __sm_bar_thr);
    dump_scheduler();
    printf_int("[bar::%d] returning\n", __sm_bar_thr);
    return;
    
    puts("[bar] now bypassing foo and calling 'a' directly");
    
    /*
     * XXX bypass foo and return directly to a, violating control flow integrity 
     * and leaving an open entry point at sm foo
     */
    //sancus_call(a.public_start, 0xFFFF);
    asm("mov #0xffff, r6\n\t"
        "mov #__sm_bar_entry, r7\n\t"
        "mov #0x0, r8\n\t"
        "br #__sm_a_entry"
        :::);
    
    SAY_HI("bar_return_from_sancus_call_attempt");
    puts("shouldn't return from my bypass attempt; now exiting...");
    EXIT
}

void SM_ENTRY("foo") enter_foo(void)
{
    printf_int("[foo::%d] enter_foo\n", __sm_foo_thr);
 
    printf_int("[foo::%d] dumping scheduler\n", __sm_foo_thr);
    dump_scheduler();

    /*
    puts("[foo] yielding...");
    yield();
    puts("[foo] return from yield :-)");
    */
    
    printf_int("[foo::%d] now calling bar\n", __sm_foo_thr);
    //XXX bar should enforce monothreading when used by SM foo thr as wel as SM A thr
    // this call should be auto-yielded and retried afterwards
    enter_bar();
    printf_int("[foo::%d] returned from enter_bar, now returning to caller\n",
        __sm_foo_thr);
    return;
}

void SM_ENTRY("a") enter_a(void)
{
    printf_int("[a::%d] enter_a\n", __sm_a_thr);    
    printf_int("[a::%d] now calling bar\n", __sm_a_thr);
    enter_bar();
    //enter_foo();
    printf_int("[a::%d] returned from bar, now returning to caller\n", __sm_a_thr);
    
    return;     // this line may return to a_private_func (via call_a_illegal_ret)
}

void SM_FUNC("a") a_private_func(void)
{
    SAY_HI("a_private_func");
    puts("\t[a_private_func] shouldn't see this; exiting...");
    EXIT
}

/**
 * XXX tries to enter SM "a" via a valid SM_ENTRY function, supplying a private
 *  SM_FUNC function as a return address
 *
 * demonstrates how to execute arbitrarily code within a SM (once, return from then
 *  on is not under the attackers control...)
 */
void SM_ENTRY("bar") call_a_illegal_ret(void)
{
    puts("\n[bar] calling SM a with an in-bounds ret addr: ");
    
    printf_int_int("a_public_start is 0x%x ; a_public_end is 0x%x",
        (int) &__sm_a_public_start, (int) &__sm_a_public_end);
    printf_int(" ; a_private_func is 0x%x\n", (intptr_t) a_private_func);
    
    extern char __sm_a_entry_enter_a_idx;
    entry_idx entryIdx = (entry_idx) &__sm_a_entry_enter_a_idx;;
    
    void *retAddr = a_private_func;
    void *entryAddr = a.public_start;
    
    asm("mov %0, r6\n\t"
        "mov %1, r7\n\t"
        "br %2"
        :
        : "m"(entryIdx), "m"(retAddr), "m"(entryAddr)
        : "r7", "r6"
    );
}

/**
 * XXX tries to call an SM with an invalid logical entry point
 */
void SM_ENTRY("bar") call_a_illegal_id(void)
{
    puts("\n[bar] calling SM a with out-of-bounds logical entry point (-1)");
    
    void *entryAddr = a.public_start;
    entry_idx entryIdx = (entry_idx) -1;
    asm("mov %0, r6\n\t"
        "br %1"
        :                                       /* output registers */
        : "m"(entryIdx), "m"(entryAddr)         /* input registers */
        : "r7","r6"                             /* clobbered registers */
    );
}

/**
 * XXX initialises its private sched_vars to be able to report return entry point
 *  abuses to the scheduler
 *
 *
 * \note:   - an SM is solely responsible to set its own private sched_vars
 *          - the exit stub goes into an infinite loop if it cannot safely contact
 *              the scheduler (i.e. addr not intialsed or ID doesn't match)
 */
void SM_ENTRY("a") set_a_vars(void)
{
    // the private vars are declared extern in the DECLARE_SM macro
    __sm_a_sad = scheduler.public_start;
    __sm_a_sid = sancus_verify(SM_GET_TAG(a, scheduler), &scheduler);
    __sm_a_vep = (entry_idx) &__sm_scheduler_entry_report_entry_violation_idx;
    __sm_a_yep = (entry_idx) &__sm_scheduler_entry_yield_idx;
    __sm_a_gep = (entry_idx) &__sm_scheduler_entry_get_cur_thr_id_idx;
    
    if(!__sm_a_sid)
        EXIT
}

void SM_ENTRY("a") print_a_cur_thr_id(void)
{
    printf_int("[a] cur_thr_id is %d\n", __sm_a_thr);
}

void SM_ENTRY("bar") print_bar_cur_thr_id(void)
{
    printf_int("[bar] cur_thr_id is %d\n", __sm_bar_thr);
}

void SM_ENTRY("foo") print_foo_cur_thr_id(void)
{
    printf_int("[foo] cur_thr_id is %d\n", __sm_foo_thr);
}

void SM_ENTRY("bar") set_bar_vars(void)
{
    __sm_bar_sad = scheduler.public_start;
    __sm_bar_sid = sancus_verify(SM_GET_TAG(bar, scheduler), &scheduler);
    __sm_bar_vep = (entry_idx) &__sm_scheduler_entry_report_entry_violation_idx;
    __sm_bar_yep = (entry_idx) &__sm_scheduler_entry_yield_idx;
    __sm_bar_gep = (entry_idx) &__sm_scheduler_entry_get_cur_thr_id_idx;
    
    if(!__sm_bar_sid)
        EXIT
}

void SM_ENTRY("foo") set_foo_vars(void)
{
    __sm_foo_sad = scheduler.public_start;
    __sm_foo_sid = sancus_verify(SM_GET_TAG(foo, scheduler), &scheduler);
    __sm_foo_vep = (entry_idx) &__sm_scheduler_entry_report_entry_violation_idx;
    __sm_foo_yep = (entry_idx) &__sm_scheduler_entry_yield_idx;
    __sm_foo_gep = (entry_idx) &__sm_scheduler_entry_get_cur_thr_id_idx;
    
    if(!__sm_foo_sid)
        EXIT
}

void do_control_flow_integrity_hack(void)
{
    puts("###### CONTROL FLOW INTEGRITY HACK ######");
    set_a_vars();
    enter_foo();            // SM bar returns to SM a, which has an empty stack
    //enter_a();              // SM bar returns to SM a, bypassing SM foo
    //call_a_illegal_id();    // call a with an out-of-bounds logical entry point id
    //call_a_illegal_ret();     // call a with an in-bounds return address
    SAY_HI("main_return_from_hack_a");
}
