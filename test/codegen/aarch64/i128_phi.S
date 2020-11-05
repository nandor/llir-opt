# RUN: %opt - -triple=aarch64


test:
  .args i32, i128, i128

  arg.i32   $0, 0
  arg.i128  $1, 1
  arg.i128  $2, 2

  jcc       $0, .Ltrue, .Lfalse
.Ltrue:
  mov.i128  $3, 10
  add.i128  $4, $1, $3
  mov.i128  $10, 100
  jmp       .Ljoin
.Lfalse:
  mov.i128  $5, 20
  add.i128  $6, $2, $5
  mov.i128  $20, 200
  jmp       .Ljoin
.Ljoin:
  phi.i128  $7, .Ltrue, $4, .Lfalse, $6
  phi.i128  $8, .Ltrue, $10, .Lfalse, $20
  add.i128  $9, $7, $8
  ret       $9
