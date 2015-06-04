#include "common.h"
#include "scheduler.h"

/**
 * A simple scheduler SM implementation for non-preemptive scheduling of multiple
 *  logical threads on a single CPU.
 *
 * \note        The implementation keeps track of all logical threads, each with
 *               __only__ their currently executing SM protection domain.
 */
DECLARE_SM(scheduler, 0x1234);

// ############################## SCHEDULER PARAM #################################

/**
 * The max number of logical threads that can be managed by the scheduler
 */
#ifndef SCHED_MAX_NB_THREADS
    #define SCHED_MAX_NB_THREADS        10
#endif 

// ############################## DATA STRUCTURES #################################

/**
 * Internal state of a logical thread:
 *
 *  REGISTERED  Not yet running, ready to be started
 *  BUSY        Now running in another logical thread, will be started later
 *  RUNNING     The currently running thread
 *  READY       Yielded, ready to be continued
 *  FINISHED    Returned successfully
 *  KILLED      SM entry violation has been reported
 *
 * \invar       At all time, there's at most one thread "RUNNING"
 */
enum thr_state { THR_REGISTERED, THR_BUSY, THR_RUNNING, THR_READY, THR_FINISHED, 
    THR_KILLED };

/**
 * Thread Control Block (TCB) representing a logical thread :
 *
 *  thr_id      The unique logical thread id, initially assigned on registration
 *  sm_id       The ID of the current SM protection domain (for debugging purposes only)
 *  pub_start   Pointer to the entry address of the current SM protection domain
 *  entry       Logical SM entry point to execute/continue this logical thread in 
 *               its current SM protection domain (0xFFFF for yielded threads)
 *  state       The thread's internal state (for debugging purposes only)
 */
struct tcb
{
    thr_id_t            thr_id;         // 2 bytes
    sm_id               sm_id;          // 2 bytes
    void*               pub_start;      // 2 bytes
    entry_idx           entry;          // 2 bytes
    enum thr_state      state;          // 2 bytes
    struct tcb          *next;          // 2 bytes
    char padding[4];                    // total = 16 bytes
};

// protected tcb memory pool
struct tcb SM_DATA("scheduler") tcb_pool[SCHED_MAX_NB_THREADS];
struct tcb SM_DATA("scheduler") *free_tcb_list;

/**
 * A queue with threads that are ready to run.
 *
 * \invar       All thr_ids in this queue are unique and not cur_thr
 * \invar       All thr_states in this queue are "REGISTERED" or "READY"
 */
struct tcb SM_DATA("scheduler") *ready_queue;
struct tcb SM_DATA("scheduler") *ready_queue_tail;

/**
 * A queue with finished and killed threads, for debugging purposes only.
 *
 * \invar       All threads in this queue are unique and not in the ready_queue
 */
struct tcb SM_DATA("scheduler") *done_queue;

/**
 * The currently executing logical thread.
 *
 * \invar       This tcb is not in the ready_queue and has state RUNNING
 * \note        On yield(), this thread is appended in the ready queue with the
 *               calling SM as its current SM protection domain
 * \note        On return, this thread is considered finished
 */
struct tcb SM_DATA("scheduler") *cur_thr;

/**
 * An internal counter, to ensure the assigned thr_ids are unique (as the scheduler
 *  SM implementation only allows initialisation once per boot cycle)
 */
thr_id_t SM_DATA("scheduler") thr_id_counter;

/**
 * Whether or not the scheduler is currently running mutltiple threads
 */
int SM_DATA("scheduler") sched_running;

int SM_DATA("scheduler") init_done;


// ############################## HELPER MACROS ###################################

#define DO_INIT \
    if (!init_done) { init_data_structures(); }

#define ALLOC_TCB(the_tcb_ptr, the_thr_id, the_sm_id, the_ps, the_entry) \
do { \
    if (!free_tcb_list) \
    { \
        printerr("no more TCB structs left, exiting..."); \
        EXIT \
    } \
    the_tcb_ptr             = free_tcb_list; \
    free_tcb_list           = free_tcb_list->next; \
    the_tcb_ptr->thr_id     = the_thr_id; \
    the_tcb_ptr->sm_id      = the_sm_id; \
    the_tcb_ptr->pub_start  = the_ps; \
    the_tcb_ptr->entry      = the_entry; \
    the_tcb_ptr->state      = THR_REGISTERED; \
    the_tcb_ptr->next       = NULL; \
} while(0)

#define FREE_TCB(the_tcb_ptr) \
do { \
    the_tcb_ptr->next = free_tcb_list; \
    free_tcb_list = the_tcb_ptr; \
} while(0)

#define READY_ENQUEUE(the_tcb) \
do { \
    if (!ready_queue) \
    { \
        ready_queue = the_tcb; \
        ready_queue_tail = ready_queue; \
    } \
    else \
    { \
        ready_queue_tail->next = the_tcb; \
        the_tcb->next = NULL; \
        ready_queue_tail = the_tcb; \
    } \
} while(0)

#define READY_DEQUEUE(the_tcb) \
do { \
    if (!ready_queue) \
        the_tcb = NULL; \
    else \
    { \
        the_tcb = ready_queue; \
        ready_queue = ready_queue->next; \
        if (!ready_queue) ready_queue_tail = NULL; \
    } \
} while(0)

// ############################## HELPER FUNCTIONS ################################

/**
 * A private func, auto called when not initialized, to ensure availability.
 */
void SM_FUNC("scheduler") init_data_structures(void)
{
    printf_int("[sched] initializing data structures; sizeof (struct tcb) is %d\n",
        sizeof(struct tcb));
    // init free tcb list
    free_tcb_list = &tcb_pool[0];
    int i; struct tcb *cur = free_tcb_list;
    for (i = 1; i < SCHED_MAX_NB_THREADS; cur = cur->next, i++)
       cur->next = &tcb_pool[i];
    cur->next = NULL;
    
    ready_queue         = NULL;
    ready_queue_tail    = NULL;
    done_queue          = NULL;
    cur_thr             = NULL;
    thr_id_counter      = 1;     // 0 is invalid thr_id
    sched_running       = 0;
    init_done           = 1;
}

// public function to be able to print the strings with unprotected functions
char *state_to_str(enum thr_state s)
{
    switch(s) {
        case THR_REGISTERED : return "REG";
        case THR_BUSY       : return "BUSY";
        case THR_RUNNING    : return "RUNNING";
        case THR_READY      : return "READY";
        case THR_FINISHED   : return "FINISHED";
        case THR_KILLED     : return "KILLED";
        default             : return "UNKNOWN";
    }
}

void SM_FUNC("scheduler") dump_tcb(struct tcb *cur)
{
    if (!cur)
    {
        puts(BOLD "\tTHREAD" NONE " (NULL)");
        return;
    }
    
    printf_int(BOLD "\tTHREAD" NONE " with thr_id %d and state " BOLD, cur->thr_id);
    printf_str(state_to_str(cur->state));
    printf_int(NONE " at %#x ; ", (intptr_t) cur);
    printf_int("next_ptr = %#x\n", (intptr_t) cur->next);
    printf_int("\t\tsm_id = %d; ", cur->sm_id);
    printf_int("pub_start = %#x ; ", (intptr_t) cur->pub_start);
    printf_int("entry = %#x\n", (intptr_t) cur->entry);
}

// ############ SCHEDULING HELPER FUNCTIONS TO BE CALLED FROM ASSEMBLY ############

#define DONE_GET_NEXT(the_done_sm, the_done_state) \
do { \
    /* keep the done thread for debugging purposes only */ \
    cur_thr->sm_id = the_done_sm; \
    cur_thr->state = the_done_state; \
    cur_thr->next = done_queue; \
    done_queue = cur_thr; \
    \
    READY_DEQUEUE(cur_thr); \
    if (!cur_thr) \
    { \
        puts("\t[sched] all logical threads seem to have finished, returning..."); \
        asm("mov #0, r15":::); \
        return;                 /* to asm stub */ \
    } \
    \
    printf_int("\t[sched] now running logical thread %d:\n", cur_thr->thr_id); \
    dump_tcb(cur_thr); \
    \
    cur_thr->state = THR_RUNNING; \
    asm("mov %0, r15\n\t" \
        "mov %1, r14" \
        : \
        : "m"(cur_thr->pub_start), "m"(cur_thr->entry) \
        : ); \
    \
    return;                     /* to asm stub */ \
} while(0)

// see scheduler.h for doc
void SM_FUNC("scheduler") finish_get_next(sm_id sm_fin)
{
    if (!sched_running)
    {
        printf_int(YELLOW "[sched] main thread SM %d returned to a non-running "
            "scheduler; will now stop execution...\n" NONE, sm_fin);
        EXIT
    }
    
    printf_int_int(YELLOW "[sched] SM %d returned, I consider logical thread "
        "%d finished now\n" NONE, sm_fin, cur_thr->thr_id);
    
    DONE_GET_NEXT(sm_fin, THR_FINISHED);
}

// see scheduler.h for doc
void SM_FUNC("scheduler") yield_get_next(sm_id sm_yield, void *sm_addr)
{
    if (!sched_running)
    {
        // simply return to caller (return addr has been checked in asm stub)
        printf_int(YELLOW "[sched] main thread SM %d yielded to a a non-running "
            "scheduler; will now return...\n" NONE, sm_yield);
        
        asm("mov %0, r15\n\t"
            "mov #0xffff, r14"
            :: "m"(sm_addr): );
        return;                     // to asm stub
    }
    
    printf_int_int(YELLOW "[sched] SM %d yielded, I will suspend logical thread "
        "%d now\n" NONE, sm_yield, cur_thr->thr_id);

    // append the current logical thread to the ready queue
    cur_thr->sm_id = sm_yield;
    cur_thr->pub_start = sm_addr;
    cur_thr->entry = 0xFFFF;
    cur_thr->state = THR_READY;
    READY_ENQUEUE(cur_thr);
    
    // get the next logical thread to execute
    READY_DEQUEUE(cur_thr);
    printf_int("\t[sched] now running logical thread %d:\n", cur_thr->thr_id);
    dump_tcb(cur_thr);
    
    cur_thr->state = THR_RUNNING;
    asm("mov %0, r15\n\t"
        "mov %1, r14"
        :
        : "m"(cur_thr->pub_start), "m"(cur_thr->entry)
        : );
 
    return;                     // to asm stub
}

// see scheduler.h for doc
void SM_FUNC("scheduler") kill_get_next(sm_id reporter, sm_id violator)
{
    if (!sched_running)
    {
        printf_int_int(YELLOW "[sched] main thread SM %d violated control flow "
            "integrity as reported by SM %d\n" NONE, violator, reporter);
        puts(YELLOW "\tscheduler not running; will now stop execution...\n" NONE);
        EXIT
    }
      
    printerr_int_int(YELLOW "[sched] SM %d reports an entry violation by SM %d\n"
        NONE, reporter, violator);
    printf_int("\t[sched] I will kill logical thread %d now\n",
        cur_thr->thr_id);
    
    DONE_GET_NEXT(violator, THR_KILLED);
}

// see scheduler.h for doc
void SM_FUNC("scheduler") busy_get_next(sm_id sm_busy)
{
    if (!sched_running)
    {
        printf_int(YELLOW "[sched] main thread SM %d returned busy to a non-running "
            "scheduler; will now stop execution...\n" NONE, sm_busy);
        EXIT
    }
    
    printf_int_int(YELLOW "[sched] portal SM %d returned magic busy value, I will "
        "restart logical thread %d later\n" NONE, sm_busy, cur_thr->thr_id);
    
    // append the current logical thread to the ready queue
    cur_thr->state = THR_BUSY;
    READY_ENQUEUE(cur_thr);
    
    // get the next logical thread to execute
    READY_DEQUEUE(cur_thr);
    printf_int("\t[sched] now running logical thread %d:\n", cur_thr->thr_id);
    dump_tcb(cur_thr);
    
    cur_thr->state = THR_RUNNING;
    asm("mov %0, r15\n\t"
        "mov %1, r14"
        :
        : "m"(cur_thr->pub_start), "m"(cur_thr->entry)
        : );
 
    return;                     // to asm stub
}

// ############################## API IMPLEMENTATION ##############################

void SM_ENTRY("scheduler") initialize_scheduler(void)
{
    DO_INIT
}

void SM_ENTRY("scheduler") dump_scheduler(void)
{
    DO_INIT
    printf_int("[sched] dumping internal state; I have SM ID %d\n",
        sancus_get_self_id());
    
    puts("ready queue:");
    puts("\t----------------------------------------------------------------");
    struct tcb *cur;
    for (cur = ready_queue; cur != NULL; cur = cur->next)
        dump_tcb(cur);
    puts("\t----------------------------------------------------------------");
    
    puts("done queue:");
    puts("\t----------------------------------------------------------------");
    for (cur = done_queue; cur != NULL; cur = cur->next)
        dump_tcb(cur);
    puts("\t----------------------------------------------------------------");
    
    puts("current thread:");
    dump_tcb(cur_thr);
    puts("");
}

int SM_ENTRY("scheduler") register_thread_portal(struct SancusModule *sm, 
    entry_idx start_fct)
{
    DO_INIT
    printf_int_int("[sched] registering new thread portal SM %d with entry %d\n",
        sm->id, start_fct); 
    
    struct tcb *new_tcb;
    ALLOC_TCB(new_tcb, thr_id_counter, sm->id, sm->public_start, start_fct);
    thr_id_counter++;
    
    READY_ENQUEUE(new_tcb);
    return SUCCESS;
}

int SM_ENTRY("scheduler") start_scheduling(void)
{
    DO_INIT
    if (sched_running)
    {
        printerr("[sched] the scheduler is already running...");
        return FAILURE;
    }
    sched_running = 1;
    
    READY_DEQUEUE(cur_thr);
    if (!cur_thr)
    {
        printerr("[sched] there are no thread portal SMs to be scheduled...");
        return FAILURE;
    }
    
    puts(YELLOW "[sched] starting scheduling of registered thread portal SMs" NONE);
    printf_int("\t[sched] now running logical thread %d:\n", cur_thr->thr_id);
    dump_tcb(cur_thr);
    
    cur_thr->state = THR_RUNNING;
    sancus_call(cur_thr->pub_start, cur_thr->entry);
    
    /**
     * sm_entry assembly stub will ensure execution is continued here iff all 
     * registered SMs have finished their execution
     */
    puts(YELLOW "[sched] All registered thread portal SMs have finished, "
        "returning to main thread..." NONE);
    sched_running = 0;
    return SUCCESS;
}

/**
 * This function should be intercepted by the dedicated asm entry_stub
 */
void SM_ENTRY("scheduler") yield(void)
{
    printerr("[yield] shouldn't see this, exiting...");
    EXIT
}

/**
 * This function should be intercepted by the dedicated asm entry_stub
 */
void SM_ENTRY("scheduler") report_entry_violation(sm_id violator)
{
    printerr("[report_violation] shouldn't see this, exiting...");
    EXIT
}

thr_id_t SM_ENTRY("scheduler") get_cur_thr_id(void)
{
    sm_id caller = sancus_get_caller_id();
    if (!cur_thr)
    {
        //printf_int(CYAN "\t[sched] returning cur_thr_id zero to SM %d\n" NONE, \
            caller);
        return 0;
    }
    
    //printf_int_int(CYAN "\t[sched] returning cur_thr_id %d to SM %d\n" NONE, \
        cur_thr->thr_id, caller);
    return cur_thr->thr_id;
}

