# RUN: %opt - -triple x86_64
# DISABLED:

test:
  .args i8, i8, i8

  arg.i8    $0, 0
  arg.i8    $1, 1
  arg.i8    $2, 2
  s_ext.i16  $3, $1
  s_ext.i16  $4, $2
  jump_cond       $0, .Ltrue, .Lfalse
.Ltrue:
  mov.i16   $5, 100
  add.i16   $6, $3, $5
  jump      .Lend
.Lfalse:
  mov.i16   $7, 100
  add.i16   $8, $4, $7
  jump      .Lend
.Lend:
  phi.i16   $9, .Ltrue, $6, .Lfalse, $8
  trunc.i8  $10, $9
  ret       $10
