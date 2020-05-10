  .text

sccp_same_phi:
  .args   0, i32

  mov.i64       $0, 2
  mov.i64       $2, 1

  arg.i32       $3, 0
  jt            $3, .Ltrue

.Lfalse:
  mov.i64       $1, 1
  add.i64       $3, $0, $1
  jmp           .Lmerge

.Ltrue:
  mov.i64       $1, 2
  add.i64       $4, $2, $1
  jmp           .Lmerge

.Lmerge:
  phi.i64       $5, .Lfalse, $3, .Ltrue, $4
  ret.i64       $5
