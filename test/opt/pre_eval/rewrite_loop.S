# RUN: %opt - -passes=pre-eval -emit=llir -entry=test -static -o=-



test:
  .visibility global_hidden
  .call c
  .stack_object   0, 8, 8
  .args           i64
.Lentry:
  frame.i64   $1, 0, 0
  mov.i64     $2, 10
  udiv.i64     $0, $1, $2
  jmp         .Lloop
.Lloop:
  phi.i64     $3, .Lentry, $2, .Lloop, $4
  mov.i64     $4, 1
  sub.i64     $5, $3, $1

  add.i64     $6, $0, $4
  st          $1, $6

  mov.i64     $7, 0
  cmp.i8.ne   $8, $5, $7
  jcc         $8, .Lloop, .Lend
.Lend:
  ld.i64      $9, $1
  ret         $9
  .end
