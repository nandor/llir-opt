# RUN: %opt - -triple x86_64

  .section .text
_raise:
  .call caml

  mov.i64 $7, 8
  mov.i64 $8, _exc_ptr
  # movq  _exc_ptr(%rip), %rax
  ld.8.i64 $3, [$8]
  # movq  -8(%rax), %rsp
  sub.i64 $4, $3, $7
  ld.8.i64 $6, [$4]
  # movq  -16(%rax), %rcx
  sub.i64 $5, $4, $7
  ld.8.i64 $1, [$5]
  # movq  %rcx, _exc_ptr(%rip)
  st [$8], $1
  # jmpq  *(%rax)
  ld.8.i64 $2, [$3]
  raise $2, $6
  .end


  .section .data
_exc_ptr:
  .quad 0

  .section .text
_try_catch:
  .call         caml
  .stack_object 0, 24, 8

  # subq  $24, %rsp
  # leaq  .L104(%rip), %rax
  mov.i64 $2, .L104
  frame.i64 $3, 0, 0
  # movq  %rax, (%rsp)
  st [$3], $2
  jmp .L102
.L102:
  mov.i64 $15, _exc_ptr
  frame.i64 $16, 0, 0
  mov.i64   $50, 0
  add.i64   $17, $16, $50
  add.i64   $18, $17, $50
  # movq  _exc_ptr(%rip), %rax
  ld.i64 $19, [$15]
  # movq  %rax, 16(%rsp)
  st [$18], $19
  # movq  %rsp, %rax
  get.i64 $20, $sp
  # movq  %rsp, 8(%rsp)
  st [$17], $20
  # movq  %rax, _exc_ptr(%rip)
  st [$15], $16

  # callq _dummy
  mov.i64 $22, _dummy
  invoke.i64.c $1002, $22, .L104  @caml_frame

  mov.i64 $23, _exc_ptr
  # movq  16(%rsp), %rcx
  ld.8.i64 $24, [$18]
  # movq  %rcx, _exc_ptr(%rip)
  st [$23], $24
  # addq  $24, %rsp
  # retq
  ret.i64 $1002

.L104:
  # xorl %eax, %eax
  # addq $24, %rsp
  # retq
  mov.i64 $0, 0
  ret.i64 $0
  .end

_dummy:
  # xorl %eax, %eax
  # retq
  mov.i64 $0, 0
  ret.i64 $0
  .end
