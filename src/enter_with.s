#include "minim.h"

# Enters the Scheme runtime from C
# Stash all callee-saved registers and setup the appropriate registers
# arguments:
#  %rdi - code address
#  %rsi - environment
# returns:
#  nothing
.globl enter_with
enter_with:
    # the usual header
    push %rbp
    mov %rsp, %rbp
    # stash callee-saved registers
    sub $0x40, %rsp
    mov %rbx, -0x8(%rbp)
    mov %r12, -0x16(%rbp)
    mov %r13, -0x20(%rbp)
    mov %r14, -0x28(%rbp)
    mov %r15, -0x30(%rbp)
    # move environment to %r9
    mov %rsi, %r9
    # setup next frame
    lea underflow_handler(%rip), %rbx
    mov %rbx, (%rsp)
    mov %rbp, -0x8(%rsp)
    mov %rsp, %rbp
    # jump
    jmp *%rdi
    # the footer
    pop %rbp
    ret
    
