# MoSingAr

MoSingAr is a tool to connect two or more Linux machines as one
machine, so that you can run programs at remote like at the local
machine.  All files; binaries, libraries, and data files, will read
from the local, but the processes are created at remote machines.

# Seccomp

The basic idea is to use seccomp to create a sandbox at remote, and
translate IO to requsts over network.  The files are cached at remote
machines for efficiency.

# playground/
 - tryseccomp.c is an example of interception of syscalls with seccomp.
