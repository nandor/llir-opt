# RUN: %opt - -triple x86_64
# DISABLED:

test:
  .args i8, i64, i64

  arg.i8    $0, 0
  arg.i64   $1, 1
  arg.i64   $2, 2
  s_ext.i64  $3, $1
  s_ext.i64  $4, $2
  jump_cond       $0, .Ltrue, .Lfalse
.Ltrue:
  mov.i128  $5, 100
  add.i128  $6, $3, $5
  jump      .Lend
.Lfalse:
  mov.i128  $7, 100
  add.i128  $8, $4, $7
  jump      .Lend
.Lend:
  phi.i128  $9, .Ltrue, $6, .Lfalse, $8
  trunc.i64 $10, $9
  ret       $10
