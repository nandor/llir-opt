  .text
phi_32:
  .args 1

  arg.i32 $0, 0
  jt $0, .Ltrue
.Lfalse:
  jmp .Lend
.Ltrue:
  jmp .Lend
.Lend:
  phi.i32 $1, .Lfalse, 0, .Ltrue, 1
  ret.i32 $1


phi_64:
  .args 1

  arg.i64 $0, 0
  jt $0, .Ltrue
.Lfalse:
  jmp .Lend
.Ltrue:
  jmp .Lend
.Lend:
  phi.i64 $1, .Lfalse, 0, .Ltrue, 1
  ret.i64 $1


phi_expand:
  .args 1

  arg.i64 $0, 0
  jt $0, .Ltrue
.Lfalse:
  mov.i64 $1, 123
  jmp .Lend
.Ltrue:
  mov.i64 $1, 124
  jmp .Lend
.Lend:
  ret.i64 $1
