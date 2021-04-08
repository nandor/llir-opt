# RUN: %opt - -pass=global-forward -emit=llir -static -entry=_start


  .section .text
_start:
  .call c
  mov.i64 $0, val
  mov.i64 $1, 1
  st      $0, $1
  st      $0, $1
  st      $0, $1
  st      $0, $1
  st      $0, $1
  st      $0, $1
  ret
  .end

  .section "data"
val:
  .quad 0
  .end
