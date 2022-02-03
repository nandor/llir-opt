# RUN: %opt - -pass=pre-eval -emit=llir -entry=test -static -o=-



  .section .text
test:
  .visibility global_hidden
  .call c
  .stack_object   0, 8, 8
  .args           i64
.Lentry:
  frame.i64   $0, 0, 0
  mov.i64     $1, 10
  u_div.i64    $2, $0, $1
  cmp.i8.ne   $3, $1, $1
  jump_cond         $3, .Ltrue, .Lfalse
.Ltrue:
  mov.i64     $20, 100
  u_div.i64    $21, $0, $20
  store          $0, $21
  jump        .Lend
.Lfalse:
  mov.i64     $30, 100
  u_div.i64    $31, $0, $30
  store          $0, $31
  jump        .Lend
.Lend:
  load.i64      $40, $0
  mov.i64     $41, data
  store          $41, $40
  ret         $40

  .section .data
data:
  .quad 0
  .end
