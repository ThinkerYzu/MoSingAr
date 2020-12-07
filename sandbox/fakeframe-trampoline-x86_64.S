
        .text
        .hidden fakeframe_trampoline
        .global fakeframe_trampoline
        .type fakeframe_trampoline, @function
fakeframe_trampoline:
        pop     %rdi
        pop     %rsi
        pop     %rbx
        pop     %rcx
        pop     %rdx
        pop     %r8
        pop     %r9

        // A hack to switch between two stacks by copy the return
	// address from this stack to the stack being restored.
        mov     16(%rsp), %r10  // rsp of the restored stack.
        mov     24(%rsp), %r11  // return address
        mov     %r11, -8(%r10)

        pop     %r10
        pop     %r11
        pop     %rsp

        sub     $8, %rsp
        retq
