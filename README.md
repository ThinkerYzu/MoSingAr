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

# Design

The directory *loader/* implements the feature to load and run a
program in a sandbox.  It comprises

 - Carrier,
 - Command Center,
 - Flight Deck, and
 - Scout (in *sandbox/*).


*Carrier* creates an evenironment to run applications.  It is the main
process to serve child processes running applications in sandboxes.

*Scouts* are running in the child processes to coordinate the sandbox
in their processes. Each of them establish a communication channel to
the *Command Center*.  The *Scout* of a process intercepts syscalls
and redirect I/O to the *Command Center* along with the messages going
through the communication channel to the *Command Center*.  *Scout* is
implemented in the *sandbox/* directory.

*Command Center* is in the *Carrier*, aka the main process.  It
handles messages from *Scouts* and serves their requests.

The *Flight Deck*, which is in the *Carrier* too, takes off *Scouts*
for processes.  Taking off a *Scout* means to start a process, if
necessary, and initialize the process to deploy a *Scout*.

## oglfs

*oglfs* implements a mechanism to synchronize and distribute files
among devices.
