/**
 * Another implementation of |syscall()| but with a bit of different.
 * |syscall()| will return -1, and put the error code in |errno|.
 * This implementation return a negative error code instead.
 */
        .text
        .hidden syscall_trampoline
        .global syscall_trampoline
        .type syscall_trampoline, @function
syscall_trampoline:
        mov     %rdi, %rax
        mov     %rsi, %rdi
        mov     %rdx, %rsi
        mov     %rcx, %rdx
        mov     %r8, %r10
        mov     %r9, %r8
        mov     8(%rsp), %r9

        syscall
        ret

/**
 * vfork is a special case since the child and the parent using the same
 * address space until next exec() at the child.  That means the data
 * might be tainted by the child process, including the return address.
 */
#define __NR_vfork 58
        .hidden vfork_trampoline
        .global vfork_trampoline
        .type vfork_trampoline, @function
vfork_trampoline:
        mov     $__NR_vfork, %rax
        popq    %rdi
        syscall
        pushq   %rdi
        ret
