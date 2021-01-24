# RUN: %opt - -passes=pre-eval -emit=llir -entry=test -static -o=-



  .section .text
test:
  .visibility global_hidden
  .call c
  .stack_object   0, 8, 8
  .args           i64
.Lentry:
  frame.i64   $0, 0, 0
  mov.i64     $1, 10
  udiv.i64    $2, $0, $1
  cmp.i8.ne   $3, $1, $1
  jcc         $3, .Ltrue, .Lfalse
.Ltrue:
  mov.i64     $20, 100
  udiv.i64    $21, $0, $20
  st          $0, $21
  jmp         .Lend
.Lfalse:
  mov.i64     $30, 100
  udiv.i64    $31, $0, $30
  st          $0, $31
  jmp         .Lend
.Lend:
  ld.i64      $40, $0
  mov.i64     $41, data
  st          $41, $40
  jmp         .Lhang
.Lhang:
  jmp         .Lhang
  .end

  .section .data
data:
  .quad 0
  .end
