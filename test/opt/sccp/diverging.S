# RUN: %opt - -pass=sccp -pass=dead-code-elim -emit=llir


diverging:
  .call     c
  .args     i8
  .visibility global_hidden

  arg.i8    $0, 0
  jump_cond $0, .Ltrue, .Lfalse
.Ltrue:
  mov.i8    $1, 100
  ret       $1
.Lfalse:
  mov.i32   $2, 100
  ret       $2
  .end


different_return:
  .call       c
  .args       i8
  .visibility global_default

  arg.i8      $0, 0

  mov.i64     $1, diverging
  call.c.i64  $2, $1, $0

  mov.i64     $3, 5
  add.i64     $4, $2, $3

  ret         $4
