# law_cor_amd64.s - coroutine for amd64 systems

#       Callee saved registers for amd64 systems.
#
#       %rbx                            general purpose
#       %rsp                            stack pointer
#       %rbp                            base pointer
#       %r12                            general purpose
#       %r13                            general purpose
#       %r14                            general purpose
#       %r15                            general purpose

.global law_cor_call_x86_64
.global law_cor_swap_x86_64

##
# INFO
#       Call the function in the coroutine's environment.
#
#       Note: this routine always returns into the initial environment
#       supplied in the arguments, even if it was suspended and resumed
#       with another initial environment (undefined behavior).
#
# ARGS
#       %rdi                            Initial environment
#       %rsi                            Coroutine environment
#       %rdx                            Function's argument
#       %rcx                            Coroutine's function
#       %r8                             Unused
#       %r9                             Unused
# RETURNS
#       %rax                            Coroutine function's return value.
#       %rdx                            Unused
law_cor_call_x86_64:
        # Push callee preserved registers to the stack.

        push %r15
        push %r14
        push %r13
        push %r12
        push %rbx

        mov %rdi, %r15                  # Store initial ptr in %r15.
        mov %rbp, 0x0(%r15)             # Store initial frame pointer.
        mov %rsp, 0x8(%r15)             # Store initial stack pointer.

        mov 0x0(%rsi), %rbp             # Load coroutine base pointer.
        mov 0x8(%rsi), %rsp             # Load coroutine stack pointer.

        call *%rcx                      # Call the function pointer.

        # %r15 is preserved so the initial pointer should still be valid.

        mov 0x0(%r15), %rbp             # Restore initial frame pointer.
        mov 0x8(%r15), %rsp             # Restore initial stack pointer.

        # Pop callee preserved registers from the stack.

        pop %rbx
        pop %r12
        pop %r13
        pop %r14
        pop %r15

        # %rax will contain the coroutine function's (%rcx) return value.

        ret                             # Return.

##
# INFO
#       Swap from source environment to destination environment.
#
# ARGS
#       %rdi                            Source environment
#       %rsi                            Destination environment
#       %rdx                            Signal integer
#       %rcx                            Unused
#       %r8                             Unused
#       %r9                             Unused
# RETURNS
#       %rax                            1
#       %rdx                            Unused
law_cor_swap_x86_64:

        # Push callee preserved registers.

        push %r15
        push %r14
        push %r13
        push %r12
        push %rbx

        mov %rbp, 0x0(%rdi)             # Store source's frame pointer.
        mov %rsp, 0x8(%rdi)             # Store source's stack pointer.

        mov 0x0(%rsi), %rbp             # Load destination's frame pointer.
        mov 0x8(%rsi), %rsp             # Load destination's stack pointer.

        # Pop callee preserved registers.

        pop %rbx
        pop %r12
        pop %r13
        pop %r14
        pop %r15

        mov %rdx, %rax                  # Set %rax to signal.
        ret                             # Return.

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
