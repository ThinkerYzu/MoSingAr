/**
 * This provides a trampoline for signal handlers.
 *
 * Signal handlers will return to the begin of this function.
 * sigreturn() should be called to restore the context.
 *
 * See:
 * man page of sigaction().
 * https://github.com/lattera/glibc/blob/master/sysdeps/unix/sysv/linux/x86_64/sigaction.c
 */
#define __NR_rt_sigreturn 15
        .text
        .hidden sig_trampoline
        .global sig_trampoline
        .type sig_trampoline, @function
sig_trampoline:
        mov     $__NR_rt_sigreturn, %rax
        syscall
