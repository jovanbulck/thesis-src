;******************************************************************************
; dedicated stub when entering the scheduler
;******************************************************************************
    .section ".sm.text"
    .align 2
    .global __sm_entry
    .type __sm_entry,@function

    ;; \arg     r6: ID of entry point to be called, 0xffff if returning
    ;;          r7: return address to caller (undefined if returning)
    ;; \return  r6: 0xffff
    ;;          r8: a magic value indicating the calee was busy (-1) or success (0)
    ;; \note    caller-save = r6, r7, r8
__sm_entry:
    ;; switch stack
    mov &__sm_sp, r1
    cmp #0x0, r1
    jne 1f
    mov #__sm_stack_init, r1

1:
    ;; ################### INTEGRITY CHECK: return address ####################
    ;; check if the given ret addr (r7) is owned by the calling SM iff not returning
    ;; SCHED: the valid ret addr is used to continue execution on a yield call
    cmp #0xffff, r6
    jeq 1f
    push r15                                    ;; backup, may contain arg
    push r14
    .word 0x1387                                ;; sancus_get_caller_id
    mov r15, r14
    mov r7, r15
    .word 0x1386                                ;; sancus_get_id
    cmp r14, r15
    pop r14
    pop r15
    jne __entry_violation                       ;; defined in sm_exit.s

1:
    ; check if this is a return
    cmp #0xffff, r6
    jne 1f
    
    ;; ################### SCHED HACK: intercept return #######################
    ;; check if this is a non-zero SM returning to the scheduler
    push r15
    .word 0x1387                                ;; sancus_get_caller_id
    cmp #0, r15
    pop r15
    jeq __ret_entry                             ;; defined in sm_exit.s
    br #__sched_ret_entry
    
1:
    ;; ################### INTEGRITY CHECK: logical entry index ###############
    ; check if the given index (r6) is within bounds
    cmp #__sm_nentries, r6
    jhs __entry_violation                       ;; defined in sm_exit.s

    ;; ########### SCHED HACK: intercept yield() and report_violaton() ########
    cmp #__sm_scheduler_entry_yield_idx, r6     ;; defined in scheduler.h
    jeq yield_call
    cmp #__sm_scheduler_entry_report_entry_violation_idx, r6
    jne normal_call

report_entry_violation:
    mov r15, r14                                ;; r14 = violator_id
    .word 0x1387                                ;; r15 = sancus_get_caller_id
    call #kill_get_next                         ;; defined in scheduler.h

    cmp #0, r15
    jne __sched_br_next                         ;; defined in sm_exit.s
    br #__sched_final_ret
    
yield_call:
    .word 0x1387                                ;; r15 = sancus_get_caller_id
    mov r7, r14
    call #yield_get_next                        ;; defined in scheduler.h
    br #__sched_br_next                         ;; defined in sm_exit.s

normal_call:
    ; store callee-save registers
    push r4
    push r5
    push r8
    push r9
    push r10
    push r11

    ; calculate offset of the function to be called (r6 x 6)
    rla r6
    mov r6, r11
    rla r6
    add r11, r6

    ; function address
    mov __sm_table(r6), r11

    ; call the internal SM logical entry point
    call r11

    ; restore callee-save registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop r5
    pop r4

    ; clear the arithmetic status bits (0, 1, 2 and 8) of the status register
    and #0x7ef8, r2

    ; clear the return registers which are not used
    mov 4+__sm_table(r6), r6
    rra r6
    jc 1f
    clr r12
    clr r13
    rra r6
    jc 1f
    clr r14
    rra r6
    jc 1f
    clr r15

1:
    mov r1, &__sm_sp
    mov #0, r8                                  ;; magic value indicating success    
    mov #0xffff, r6
    br r7                                       ;; return to caller
