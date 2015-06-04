#include "common.h"
#include "br.h"

void SM_FUNC("bar") bar_sm_func(void)
{
    SAY_HI("bar_sm_func");
}

void SM_ENTRY("bar") bar_legal_entry(void)
{
    SAY_HI("bar_legal_entry");
}

void myfct(void)
{
    SAY_HI("return_addr_fct");
}

void do_return_addr_hack(void)
{
    puts("###### RET ADDR HACK ######");
    puts("calling bar_legal_entry with hacked return addr");

    extern char __sm_bar_entry_bar_legal_entry_idx;
    entry_idx entryIdx = (entry_idx) &__sm_bar_entry_bar_legal_entry_idx;
    
    void *retAddr = (void*) &bar_sm_func;
    //void *retAddr = (void*) &myfct;
    void *entryAddr = bar.public_start;
    
    asm("mov %0, r6\n\t"
        "mov %1, r7\n\t"
        "br %2"
        :                                                   /* output registers */
        : "m"(entryIdx), "m"(retAddr), "m"(entryAddr)       /* input registers */
        : "r7", "r6"                                        /* clobbered registers */
    );
}
