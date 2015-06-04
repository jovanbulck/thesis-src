## Secure Scheduling

This directory contains the source code for the Sancus threading model and scheduler
implementation:

* __sched__: holds the C source code of the scheduler and a minimal test set-up

* __spm-compiler__: holds the modified files from the Sancus compiler git repo and
the compiler-generated `sm_entry` and `sm_exit` stubs for participating threading-aware
SMs, as well as the dedicated scheduler stubs

* __logs__: some dumps of some minimal test scenarios (note: the logs are structured
through ANSI color escape codes that should be interpreted by a terminal e.g. `cat log.txt`)
