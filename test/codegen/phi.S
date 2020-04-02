  .code
phi_32:
  .args   0, i32

  arg.i32 $0, 0
  jt $0, .Ltrue_0
.Lfalse_0:
  jmp .Lend_0
.Ltrue_0:
  jmp .Lend_0
.Lend_0:
  phi.i32 $1, .Lfalse_0, 0, .Ltrue_0, 1
  ret.i32 $1


phi_64:
  .args 0, i64

  arg.i64 $0, 0
  jt $0, .Ltrue_1
.Lfalse_1:
  jmp .Lend_1
.Ltrue_1:
  jmp .Lend_1
.Lend_1:
  phi.i64 $1, .Lfalse_1, 0, .Ltrue_1, 1
  ret.i64 $1


phi_expand:
  .args 0, i64

  arg.i64 $0, 0
  jt $0, .Ltrue_2
.Lfalse_2:
  mov.i64 $1, 123
  jmp .Lend_2
.Ltrue_2:
  mov.i64 $1, 124
  jmp .Lend_2
.Lend_2:
  ret.i64 $1


phi_f32:
  .args 0, i64

  arg.i64 $0, 0
  jt $0, .Ltrue_3
.Lfalse_3:
  jmp .Lend_3
.Ltrue_3:
  jmp .Lend_3
.Lend_3:
  phi.f32 $1, .Lfalse_3, 0, .Ltrue_3, 0xC2F63EFA
  ret.f32 $1



