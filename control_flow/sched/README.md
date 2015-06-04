## Scheduler Set-Up

- __scheduler.h__: the documented interface of the SM_sched module

- __scheduler.c__: contains the SM_sched module's C implementation, a simple circular
FIFO scheduling policy with a ready queue; the C functions are called from the
dedicated assembly stubs (in ../spm-compiler/asm_sched)

- __foobar.c__: bunch of SMs to test control flow integrity and threading
(see ../logs for some dumps of some scenarios)
