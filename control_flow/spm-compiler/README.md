## Modified SPM-compiler files

This directory contains all modified files from the Sancus compiler git repo:

* __asm_original__: contains the original unmodified `sm_entry` and `sm_exit` stubs
automatically inserted by the Sancus compiler (included here for ease of comparison)

* __asm_sm__: contains the `sm_entry` and `sm_exit` stubs for cooperating SMs that
guard the entry of their protection domain to check whether:
    
    1. the provided logical entry point id is in bounds
    2. the provided ret address belongs to the caller
    3. on return: there is an open return entry point and the saved callee smID
    matches the caller's smID

If any of the above doesn't match, the callee SM notifies the scheduler iff the
scheduler has been registered internally and it is still loaded at the expected address

SMs are made threading-aware by:

   1. retrieving and comparing the current thrID on every entry
   2. returning a magic value indicating success or busy (i.e. executing for another
logical thread) in r8 on every exit
   3. checking r8 on every return and yield to the scheduler when the callee was busy

* __asm_sched__: contains the dedicated `sm_entry` and `sm_exit` stubs of the scheduler
that glue everything together on top of the existing SM call/return scheme. On entry,
these stubs:

    1. intercept return, yield and report_violation calls
    2. call a C helper function to make the scheduling decision
    3. branch to the next SM or return to the main thread

* __linker.py__: modified linker script to:

    * reserve space for private vars in every SM so that the scheduler's contact
    information can be stored securely and accessed from the assembly stubs.
    * allow a user-defined path for the `sm_entry` and `sm_exit` stubs of the module
    named "scheduler"

* __sm_support.h__: added definitions to be able to access the added private scheduler
information vars from C

* __SancusModuleCreator__: to adopt the `unprotected_exit` stub so that:

    * r8 is saved as a caller-save register (since it holds the busy indicator on return)
    * if the unprotected domain is made threading-aware, it should also auto-yield
    here when r8 indicates the callee SM is busy...
