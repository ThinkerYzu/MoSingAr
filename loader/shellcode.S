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

        .hidden shellcode_funcall_trap
        .global shellcode_funcall_trap
        .type shellcode_funcall_trap, @function
        .align  8
shellcode_funcall_trap:
        /* Always skip 2 nop to avoid alignment with the page, or
	 * it may cause segfault.
         *
         * For some reason, it may cause segfault if first instruction
	 * is at the first byte of a map, the bytes before the map is
	 * not mapped yet.
         */
        nop
        nop

        call    *.L1(%rip)
        int3                    /* trap */

        .align  8
.L1:
        .quad  0                /* the entry point of the function */

        .hidden shellcode_funcall_trap_end
        .global shellcode_funcall_trap_end
        .type shellcode_funcall_trap_end, @function
shellcode_funcall_trap_end:

