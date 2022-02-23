# RUN: %opt - -triple x86_64

has_cfi:
  .call         c
  .args         i64, i64

  arg.i64       $0, 0
  arg.i64       $1, 1
  add.i64       $2, $0, $1

  mov.i64       $3, callee
  call.c.i64    $4, $3, $2

  ret           $4
  .end

