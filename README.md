## Secure Resource Sharing for Embedded Protected Module Architectures

The work is based upon Sancus (https://distrinet.cs.kuleuven.be/software/sancus/) a hardware-only Protected Module Architecture for small embedded devices. The source code presented here extends Sancus with (i) a protected file system with SM-grained access control guarantees, and (ii) a secure multithreading model and accompanying unprivileged scheduler.

* __access_control__: contains the source code on Sancus-module-grained logical file access control

* __control_flow__: contains the source code on secure control flow and threading in Sancus
