#include "minim.h"

# Processes arguments to make a rest arguments
# arguments:
#  %rbp - base pointer of frame
#  %rsi - return address
#  %rdi - required argument count
#  %r14 - actual argument count
# temporaries:
#  %rbx - scratch
#  %r12 - return address
#  %r13 - first argument to rest argument
#  %r14 - iterator over argument stack
# result:
#  rest argument as a list
.globl do_rest_arg
do_rest_arg:
    # end of the rest argument
    mov minim_null(%rip), %rax
    # special case: no arguments (just return)
    cmp %rdi, %r14
    jne .L0
    jmp *%rsi

.L0:
    # otherwise %r14 >= 1
    # recall that the frame has 2 words before the arguments
    # compute stack pointer since we jump to C
    # (maintain alignment with branchless increment, h/t Godbolt)
    mov %r14, %rbx
    andl $1, %ebx
    lea 2(%rbx, %r14), %rbx
    neg %rbx
    lea (%rbp, %rbx, 8), %rsp
    # stash return address
    mov %rsi, %r12
    # first argument to rest argument
    add $2, %rdi
    neg %rdi
    lea (%rbp, %rdi, 8), %r13
    # 1 past last argument to rest argument
    add $2, %r14
    neg %r14
    lea (%rbp, %r14, 8), %r14

.L1:
    # increment pointer
    add $8, %r14
    # call `Mcons`
    mov (%r14), %rdi
    mov %rax, %rsi
    call Mcons
    # loop if %r14 <= %r13
    cmp %r13, %r14
    jl .L1

.L2:
    # jump back to procedure
    jmp *%r12
