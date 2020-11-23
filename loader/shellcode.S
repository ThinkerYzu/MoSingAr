/**
 * This shell code will be injected to the target process to
 * allocate a block of memory with executible permission,
 * load stage2 to the block, call it.
 */
        .text
        .hidden shellcode_syscall
        .global shellcode_syscall
        .type shellcode_syscall, @function
shellcode_syscall:
        syscall
        int3                    /* trap */

        .hidden shellcode_syscall_end
        .global shellcode_syscall_end
        .type shellcode_syscall_end, @function
shellcode_syscall_end:
