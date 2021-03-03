# RUN: %opt - -triple x86_64


intr:
  .call intr

  mov.i64   $0, f
  call.c    $0

  ret
