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
