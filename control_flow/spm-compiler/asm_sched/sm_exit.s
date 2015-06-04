;******************************************************************************
; stub that will be inserted on every external call
;******************************************************************************
    .section ".sm.text"
    .align 2
    .global __sm_exit
    .type __sm_exit,@function

    ; r6: ID of logical entry point or function address to be called
    ; r7: bitmask indicating which registers are used for arguments
    ; r8: physical entry point to call
__sm_exit:
    ; store and clear callee-save registers
    push r4
    clr  r4
    push r5
    clr  r5
    push r9
    clr  r9
    push r10
    clr  r10
    push r11
    clr  r11

    ; clear unused argument registers
    rra r7
    jc 1f
    clr r12
    rra r7
    jc 1f
    clr r13
    rra r7
    jc 1f
    clr r14
    rra r7
    jc 1f
    clr r15
1:
    ; store sp
    mov r1, &__sm_sp
    ; branch to the physical entry point
    mov #__sm_entry, r7
    br r8

;******************************************************************************
; stub that will be inserted on every zero-SM re-entry
;******************************************************************************
    .align 2
    .global __ret_entry
    .type __ret_entry,@function

    ;; stack has been restored by sm_entry
    ;; no need to save and check callee SM, as it is always zero (unprotected SM)
    ;; no need to check busy value as it is always ok (unprotected SM)
__ret_entry:
    ;; if there's no open entry point, the stack is empty
    cmp #__sm_stack_init, r1
    jeq __entry_violation

do_normal_return:    
    ; restore callee-save registers
    pop r11
    pop r10
    pop r9
    pop r5
    pop r4
    
    pop r8
    pop r7
    pop r6
    ret                                 ;; continue internal execution

;******************************************************************************
; SCHED dedicated return entry point
;******************************************************************************
    .align 2
    .global __sched_ret_entry
    .type __sched_ret_entry,@function
    ;; r8: magic busy value returned by callee
    ;; (ignore any rv in r15 since finished threads do not return values)
__sched_ret_entry:
    .word 0x1387                        ;; r15 = sancus_get_caller_id
    cmp #0, r8
    jeq 1f
    call #busy_get_next                 ;; defined in scheduler.h
    jmp __sched_br_next
    
1:
    call #finish_get_next               ;; defined in scheduler.h
    cmp #0, r15
    jne __sched_br_next
    jmp __sched_final_ret

;******************************************************************************
; SCHED branch to next SM
;******************************************************************************
    .align 2
    .global __sched_br_next
    .type __sched_br_next,@function
    ;; r14: logical entry idx of SM to call
    ;; r15: physical entry point of SM to call
    ;; stack has been restored by sm_entry and is untouched
__sched_br_next:
    ;; (no need to clear registers as the scheduler has no "secret" state)
    mov r1, &__sm_sp
    mov r14, r6
    mov #__sm_entry, r7
    mov #0, r8                                  ;; magic value indicating success
    br r15

;******************************************************************************
; SCHED final return to main thread
;******************************************************************************
    .align 2
    .global __sched_final_ret
    .type __sched_final_ret,@function
    ;; no next SM to call, return to after the point where the first SM was launched    
__sched_final_ret:
    jmp do_normal_return

;******************************************************************************
; SCHED kill current violating thread
;******************************************************************************
    .align 2
    .global __entry_violation
    .type __entry_violation,@function
    ;; stack has been restored by sm_entry and is untouched
__entry_violation:
    .word 0x1387                                ;; sancus_get_caller_id
    mov r15, r14                                ;; r14 = violator
    mov r0, r15
    .word 0x1386                                ;; r15 = sancus_get_id = reporter
    call #kill_get_next                         ;; defined in scheduler.h

    cmp #0, r15
    jne __sched_br_next                         ;; defined in sm_exit.s
    br #__sched_final_ret
