# RUN: %opt - -pass=pre-eval -emit=llir

  .section .text
main:
  mov.i64   $0, a
  load.i64    $1, [$0]
  mov.i64   $2, 1
  add.i64   $3, $1, $2
  store.i64    [$0], $3
  ret.i64   $3
  .end


  .section .data
  .align 8
a:
  .quad 0
