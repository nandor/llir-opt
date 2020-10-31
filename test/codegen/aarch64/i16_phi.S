# RUN: %opt - -triple x86_64
# DISABLED:

test:
  .args i8, i8, i8

  arg.i8    $0, 0
  arg.i8    $1, 1
  arg.i8    $2, 2
  sext.i16  $3, $1
  sext.i16  $4, $2
  jcc       $0, .Ltrue, .Lfalse
.Ltrue:
  mov.i16   $5, 100
  add.i16   $6, $3, $5
  jmp       .Lend
.Lfalse:
  mov.i16   $7, 100
  add.i16   $8, $4, $7
  jmp       .Lend
.Lend:
  phi.i16   $9, .Ltrue, $6, .Lfalse, $8
  trunc.i8  $10, $9
  ret       $10
