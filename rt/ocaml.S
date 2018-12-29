  .globl _caml_gc_regs
  .globl _caml_garbage_collection

  .globl _caml_collect
_caml_collect:
  pushq   %rbp
  pushq   %r11
  pushq   %r10
  pushq   %r13
  pushq   %r12
  pushq   %r9
  pushq   %r8
  pushq   %rcx
  pushq   %rdx
  pushq   %rsi
  pushq   %rdi
  pushq   %rbx
  pushq   %rax
  mov     %rsp, _caml_gc_regs(%rip)

  subq    $(16*8), %rsp
  movsd   %xmm0, 0*8(%rsp)
  movsd   %xmm1, 1*8(%rsp)
  movsd   %xmm2, 2*8(%rsp)
  movsd   %xmm3, 3*8(%rsp)
  movsd   %xmm4, 4*8(%rsp)
  movsd   %xmm5, 5*8(%rsp)
  movsd   %xmm6, 6*8(%rsp)
  movsd   %xmm7, 7*8(%rsp)
  movsd   %xmm8, 8*8(%rsp)
  movsd   %xmm9, 9*8(%rsp)
  movsd   %xmm10, 10*8(%rsp)
  movsd   %xmm11, 11*8(%rsp)
  movsd   %xmm12, 12*8(%rsp)
  movsd   %xmm13, 13*8(%rsp)
  movsd   %xmm14, 14*8(%rsp)
  movsd   %xmm15, 15*8(%rsp)

  callq   _caml_garbage_collection

  movsd   0*8(%rsp), %xmm0
  movsd   1*8(%rsp), %xmm1
  movsd   2*8(%rsp), %xmm2
  movsd   3*8(%rsp), %xmm3
  movsd   4*8(%rsp), %xmm4
  movsd   5*8(%rsp), %xmm5
  movsd   6*8(%rsp), %xmm6
  movsd   7*8(%rsp), %xmm7
  movsd   8*8(%rsp), %xmm8
  movsd   9*8(%rsp), %xmm9
  movsd   10*8(%rsp), %xmm10
  movsd   11*8(%rsp), %xmm11
  movsd   12*8(%rsp), %xmm12
  movsd   13*8(%rsp), %xmm13
  movsd   14*8(%rsp), %xmm14
  movsd   15*8(%rsp), %xmm15
  addq    $(16*8), %rsp

  popq    %rax
  popq    %rbx
  popq    %rdi
  popq    %rsi
  popq    %rdx
  popq    %rcx
  popq    %r8
  popq    %r9
  popq    %r12
  popq    %r13
  popq    %r10
  popq    %r11
  popq    %rbp
  ret
