# RUN: %opt - -triple x86_64

  .section .text
caller:
  mov.i64 $0, _callee
  call.c  $0
  ret
