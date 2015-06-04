;******************************************************************************
; stub that will be executed on every external call
;******************************************************************************
    .section ".sm.text"
    .align 2
    .global __sm_exit
    .type __sm_exit,@function

    ; r6: ID of logical entry point or function address to be called
    ; r7: bitmask indicating which registers are used for arguments
    ; r8: physical entry point to call
    ; stack = ret, r6, r7, r8
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
    ;; store logical and physical entry point (to retry after yield for busy callee)
    push r6
    push r8
    
    ;; ################### INTEGRITY SAVE: callee SM id #######################
    mov r15, r4                         ;; backup r15, may hold arg
    mov r8, r15
    .word 0x1386                        ;; sancus_get_id
    push r15
    mov r4, r15                         ;; restore r15
    
    ;; r6: ID of logical entry point or function address to be called
    ;; r8: physical entry point to call
    ;; stack = ret, r6, r7, r8, r4, r5, r9, r10, r11, log, phys, callee (top)
do_exit_call:
    ; store sp
    mov r1, &__sm_sp
    ; branch to the physical entry point
    mov #__sm_entry, r7
    br r8
    
;******************************************************************************
; stub that will be returned to after a yield caused by a busy callee
;******************************************************************************
    .align 2
    .global __sched_ret_entry
    .type __sched_ret_entry,@function
    
    ;; stack = ret, r6, r7, r8, r4, r5, r9, r10, r11, log, phys, callee (top)
__sm_retry_exit_call:
    pop r4
    pop r8
    pop r6
    push r6
    push r8
    push r4
    jmp do_exit_call

;******************************************************************************
; stub that will be executed on every re-entry attempt
;******************************************************************************
    .align 2
    .global __ret_entry
    .type __ret_entry,@function

    ;; stack has been restored and calee id verified by sm_entry
    ;; stack = ret, r6, r7, r8, r4, r5, r9, r10, r11, log, phys, callee (top)
__ret_entry:
    ;; ############## THREADING CHECK: check callee could accept call #########
    ;; check magic busy value (r8) returned by callee
    cmp #0, r8
    jne do_yield
    
    ;; discard the callee id and logical and physical entry points
    pop r4
    pop r4
    pop r4

    ; restore callee-save registers
    pop r11
    pop r10
    pop r9
    pop r5
    pop r4

    ;; (restore caller-save regs pushed by compiler before __sm_exit)
    pop r8
    pop r7
    pop r6
    ret                                 ;; continue internal execution

;******************************************************************************
; stub that will be executed after calling busy SM
;******************************************************************************
    ;; call the registered scheduler's yield entry point iff valid addr and ID
    ;; stack = ret, r6, r7, r8, r4, r5, r9, r10, r11, log, phys, callee (top)
do_yield:
    cmp #0x0, &__sm_sad
    jeq nok
    mov &__sm_sad, r15
    .word 0x1386                        ;; sancus_get_id
    cmp r15, &__sm_sid
    jne nok

    ;; push things for the yield call to the scheduler
    push #__sm_retry_exit_call
    push r6
    push r7
    push r8
    mov &__sm_yep, r6
    mov #0, r7
    mov &__sm_sad, r8
    br #__sm_exit                       ;; returns to pushed ret addr on re-entry

nok:
    ;; something went wrong when contacting scheduler, go into an infinite loop
    br #exit
