;******************************************************************************
; stub that will be executed when entering an SM
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
    cmp #0xffff, r6
    jne do_args_check

    ;; ################### INTEGRITY CHECK: callee SM id ######################
    ;; verify the saved callee SM id matches the SM that returned here now; before 
    ;; calling the scheduler for get_cur_thr_id as sancus_get_caller_id changes

    ;; if there's no open entry point, the stack is empty
    cmp #__sm_stack_init, r1
    jeq __entry_violation
    pop r7                              ;; (r7 = caller-save and not used here)
    push r15
    .word 0x1387                        ;; sancus_get_caller_id
    cmp r15, r7
    pop r15
    ;; push callee id back to leave this re-entry point open for now
    push r7
    jne __entry_violation
    jmp do_thr_check                    ;; no args on valid returning here

do_args_check:
    ;; ################### INTEGRITY CHECK: logical entry index ###############
    ; check if the given index (r6) is within bounds
    cmp #__sm_nentries, r6
    jhs __entry_violation
    
    ;; ################### INTEGRITY CHECK: return address ####################
    ;; check the provided ret addr (r7) is owned by the calling SM
    push r15
    push r14
    .word 0x1387                        ;; sancus_get_caller_id
    mov r15, r14
    mov r7, r15
    .word 0x1386                        ;; sancus_get_id
    cmp r14, r15
    pop r14
    pop r15
    jne __entry_violation

do_thr_check:
    ;; ############## THREADING CHECK: ensure internal monothreading ##########
    ;; empty stack means this SM can execute in a new thread
    cmp #__sm_stack_init, r1
    jeq save_cur_thr_id

    ;; pass a return from the scheduler; it might be the answer of get_cur_thr_id
    ;; note we should still check a normal sched call to start a new thread
    cmp #0xffff, r6
    jne cmp_cur_thr_id
    push r15
    .word 0x1387                        ;; sancus_get_caller_id
    cmp r15, &__sm_sid
    pop r15
    jeq cur_thr_id_ok

cmp_cur_thr_id:
    push r15
    call #__get_cur_thr_id
    cmp r15, &__sm_thr
    pop r15
    jeq cur_thr_id_ok
    
    ;; another thread wants to call this busy SM, return magic value to caller
    ;; (caller ret addr was checked above iff not returning)
    cmp #0xffff, r6
    jeq __entry_violation               ;; other thread tried to return here
    mov r1, &__sm_sp
    mov #0xffff, r6
    mov #-1, r8                         ;; magic value indicating busy
    br r7                               ;; return to caller

save_cur_thr_id:
    push r15
    call #__get_cur_thr_id
    mov r15, &__sm_thr
    pop r15

cur_thr_id_ok:
    ; check if this is a return
    cmp #0xffff, r6
    jne 1f
    br #__ret_entry                     ;; defined in sm_exit.s
    
1:
    ; store callee-save registers
    push r4
    push r5
    ;; push r8 ;; r8 is caller-save now
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
    ;pop r8 ;; r8 is caller-save now
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
    mov #0, r8                          ;; magic value indicating success    
    mov #0xffff, r6
    br r7                               ;; return to caller

;******************************************************************************
; stub that will be executed on get_cur_thr_id request
;******************************************************************************
    .align 2
    .global __get_cur_thr_id
    .type __get_cur_thr_id,@function

    ;; \arg     return address on top of stack
    ;; \return  r15 = cur_thr_id (0 when scheduler has not been registered)
__get_cur_thr_id:
    cmp #0x0, &__sm_sad
    jeq ret_zero
    mov &__sm_sad, r15
    .word 0x1386                        ;; sancus_get_id
    cmp r15, &__sm_sid
    jne .Lerror

    ;; store caller-save registers
    push r6
    push r7
    push r8
    mov &__sm_gep, r6
    mov #0, r7
    mov &__sm_sad, r8
    br #__sm_exit                       ;; will return to caller on re-entry

ret_zero:
    mov #0, r15
    ret

;******************************************************************************
; stub that will be executed on every (re)entry protocol violation
;******************************************************************************
    .align 2
    .global __entry_violation
    .type __entry_violation,@function
    
    ;; stack has been restored by sm_entry and is untouched
__entry_violation:
    mov r1, &__sm_sp
    ;; report entry violation to the scheduler, iff addr intialized and correct ID
    cmp #0x0, &__sm_sad
    jeq .Lerror
    mov &__sm_sad, r15
    .word 0x1386                        ;; sancus_get_id
    cmp r15, &__sm_sid
    jne .Lerror
    
    .word 0x1387                        ;; sancus_get_caller_id (i.e. violating SM)
    mov &__sm_vep, r6
    ;; (sched won't return, but its entry stub requires a valid return addr anyway)
    mov #__sm_entry, r7
    br &__sm_sad

.Lerror:
    ;; something went wrong when contacting scheduler, go into an infinite loop
    br #exit
