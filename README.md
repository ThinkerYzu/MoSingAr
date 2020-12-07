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

# Try

Running *make test* at the root of the repository will run the
existing test cases.  *loader/carrier* is the main program to bring up
an environment to run programs.  For example,

    LD_LIBRARY_PATH=../sandbox ./carrier gcc -c tests/hello.cpp

is the command to compile *hello.cpp* with GCC.

This tools is still incomplete, only works with a sandbox to intercept
some syscalls.  It is still needed to work on to implement network
features.
