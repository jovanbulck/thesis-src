#ifndef SM_SCHEDULER_H
#define SM_SCHEDULER_H

/**
 * A secure scheduler SM interface to interweave execution of multiple logical threads
 *  on a single CPU.
 *
 * \note        A logical thread consists of a thread "portal" SM that may call
 *               arbitrarily many other SMs.
 * \note        Different logical threads may call the same SM, but SMs themselves
 *               ensure monothreading (i.e. they execute for at most one logical
 *               thread at all time)
 * \note        SMs cooperate in the sense that they yield from time to time and 
 *               ensure control flow integrity by (1) executing for at most one
 *               logical thread at all time and (2) notifying the scheduler on 
 *               entry protocol violation
 * \note        The scheduler bootstraps on the existing function call/return 
 *               mechanism for SMs (i.e. 'continue' is 'return from yield')
 * \note        As there's no preemption via a secure HW exception engine, an SM may
 *               still claim the CPU by never yielding or returning
 */
extern struct   SancusModule scheduler;


// ############################## PUBLIC API ##############################
// public API to be called from the main thread, intializing the scheduling

/**
 * Ensures the scheduler is properly initialised (but not more than once).
 */
void SM_ENTRY("scheduler") initialize_scheduler(void);

/**
 * For debugging purposes; dumps the internal scheduler state on stdout.
 */
void SM_ENTRY("scheduler") dump_scheduler(void);

/**
 * A function that can be used by the main thread to register an SM as the "portal"
 *  of a new logical thread, to be started later.
 *
 * \arg sm          The thread "portal" SM of the new logical thread.
 * \arg start_fct   The logical entry point to be called when the portal SM is started
 * \return          a value >= 0 on success, else a negative value
 *
 * \note            When the start_fct returns to the scheduler, the corresponding
 *                  logical thread is considered to be finished.
 * \note            Finished threads do not return values.
 */
int SM_ENTRY("scheduler") register_thread_portal(struct SancusModule *sm, 
    entry_idx start_fct);

/**
 * A function that can be used by the main thread to start scheduling all previously
 *  registered SMs.
 *
 * \return          This function returns a value >= 0 when all registered SMs have
 *                  successfully finished.
 */
int SM_ENTRY("scheduler") start_scheduling(void);


// ############################## PUBLIC API ##############################
// public API to be called from the SMs being scheduled

// entry idx definitions (to be able to initialise the asm stub vars)
extern char     __sm_scheduler_entry_report_entry_violation_idx;
extern char     __sm_scheduler_entry_yield_idx;
extern char     __sm_scheduler_entry_get_cur_thr_id_idx;

/**
 * An entry point that can be called from a currently running SM to indicate it
 *  should be suspended and the next one can be run.
 *
 * \return          When this function returns, the calling SM may continue running
 *
 * \note            When calling this function, the caller is responsible to save the
 *                  callee-save registers himself (this is not needed for SMs as they
 *                  already save and clear all registers)
 */
void SM_ENTRY("scheduler") yield(void);

/**
 * An entry point that can be called from an sm_entry or sm_exit asm stub to report
 *  an inapproprate (re-)entry in the calling SM.
 *
 * \arg violator    r15 = ID of the SM that called the reporting SM, i.e. the one
 *                  that violated the entry protocol
 * \return          This function does _not_ return (!)
 *
 * \note            Possible violations of the SM entry protocol are :
 *                   1. logical entry point ID out of bounds
 *                   2. return entry point specified by a non-callee SM
 *                   3. return address within SM bounds
 */
void SM_ENTRY("scheduler") report_entry_violation(sm_id violator);

/**
 * A typedef uniquely identifying a logical thread during a boot cycle.
 *
 * \note            Id 0 is reserved to indicate there's no currently executing thread
 * \note            Implementation ensures thr_ids are not reused so that SMs can
 *                  rely on them to ensure they only work for one thread at all time
 *                  (even if the thread was killed or spoofed finishing...)
 */
typedef unsigned thr_id_t;

/**
 * An entry point that may be used to retrieve the currently executing thread id.
 *
 * \return          r15 = unique thr_id of the currently executing thread, 0 if none
 *
 * \note            This function may be used by an sm_entry asm stub to ensure the
 *                  SM is executing for at most one logical thread at all time.
 */
thr_id_t SM_ENTRY("scheduler") get_cur_thr_id(void);


// ############################## PRIVATE API ##############################
// private API to be called from the scheduler's dedicated assembly stubs
// these helper functions make the actual scheduling ("who's next") decisions

/**
 * A private fct, called by the scheduler's dedicated sm_entry stub when the
 *  currently executing logical thread returns (from a non-zero SM protection domain)
 *
 * This fct updates the scheduler's internal state and returns which SM to call next
 *  or indicates all logical threads have finished.
 *
 * \arg sm_ready    r15 = ID of the SM that returned to the scheduler
 * \return          r15 = sm_pub_start of the next SM to call (0x00 if no remaining)
 *                  r14 = sm_fct_idx of the next SM to call (if any)
 *
 * \note            This fct is only called when a protected (non-zero) SM returns.
 *                  The scheduler may still call non-protected functions (eg printf)
 */
void SM_FUNC("scheduler") finish_get_next(sm_id sm_fin);

/**
 * A private fct, called by the scheduler's dedicated sm_entry stub when the
 *  currently executing logical thread yields
 * 
 * This fct updates the scheduler's internal state and returns which SM to call next
 *
 * \arg sm_ready    r15 = ID of the SM that returned to the scheduler
 * \arg sm_addr     r14 = sm_pub_start of the SM that just yielded
 * \return          r15 = sm_pub_start of the next SM to call
 *                  r14 = sm_fct_idx of the next SM to call
 *
 * \note            This function always returns a SM to call next, if no others are
 *                  ready, it returns the SM that just yielded
 */
void SM_FUNC("scheduler") yield_get_next(sm_id sm_yield, void *sm_addr);

/**
 * A private fct, called by the scheduler's dedicated sm_entry stub when the
 *  currently executing logical thread reports an entry protocol violation
 *
 * This fct updates the scheduler's internal state and returns which SM to call next
 *  or indicates all logical threads have finished.
 *
 * \arg reporter    r15 = the SM that reported the entry violation
 * \arg violator    r14 = the SM that allegedly violated the entry protocol
 * \return          r15 = sm_pub_start of the next SM to call (0x00 if no remaining)
 *                  r14 = sm_fct_idx of the next SM to call (if any)
 *
 * \note            The currently executing thread cannot be continued anymore since
 *                  control flow integrity is broken. The scheduler will remove the
 *                  logical thread from it's ready queue.
 * \note            SMs can still ensure internal monothreading since the thr_id of
 *                  the killed thread will not be re-used.
 * \note            Any SM in the currently executing thread may report an allegedly
 *                  entry violation, effectively killing the thread. A calling SM
 *                  therefore trusts the callee not to kill the current thread.
 */
void SM_FUNC("scheduler") kill_get_next(sm_id reporter, sm_id violator);

/**
 * A private fct, called by the scheduler's dedicated sm_exit stub when the
 *  a newly launched logical thread returns, indicating it cannot accept the
 *  start function call now, as it is already executing in another logical thread.
 *
 * This fct updates the scheduler's internal state and returns which SM to call next
 *
 * \arg sm_busy     r15 = ID of the SM that returned a magic busy value
 * \return          r15 = sm_pub_start of the next SM to call
 *                  r14 = sm_fct_idx of the next SM to call
 *
 * \note            This function always returns a SM to call next, if sm_busy is
 *                  the only remaining logical thread, it returns sm_bust anyway (as
 *                  it should not be busy and is therefore not cooperating).
 */
void SM_FUNC("scheduler") busy_get_next(sm_id sm_busy);

#endif //SM_SCHEDULER_H
