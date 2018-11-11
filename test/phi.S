  .text
phi_32:
  arg.i32 $0, 0
  jt $0, .Ltrue
.Lfalse:
  jmp .Lend
.Ltrue:
  jmp .Lend
.Lend:
  phi.i32 $1, .Lfalse, 0, .Ltrue, 0
  ret.i32 $1
