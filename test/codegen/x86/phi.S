  .text
phi_32:
  .args   i32

  arg.i32 $0, 0
  jt $0, .Ltrue_0
.Lfalse_0:
  mov.i32 $10, 0
  jmp .Lend_0
.Ltrue_0:
  mov.i32 $11, 1
  jmp .Lend_0
.Lend_0:
  phi.i32 $1, .Lfalse_0, $10, .Ltrue_0, $11
  ret.i32 $1


phi_64:
  .args   i64

  arg.i64 $0, 0
  jt $0, .Ltrue_1
.Lfalse_1:
  mov.i64 $10, 0
  jmp .Lend_1
.Ltrue_1:
  mov.i64 $11, 1
  jmp .Lend_1
.Lend_1:
  phi.i64 $1, .Lfalse_1, $10, .Ltrue_1, $11
  ret.i64 $1


phi_expand:
  .args i64

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
  .args i64

  arg.i64 $0, 0
  jt $0, .Ltrue_3
.Lfalse_3:
  mov.f32   $10, 0
  jmp .Lend_3
.Ltrue_3:
  mov.f32   $11, 0xC2F63EFA
  jmp .Lend_3
.Lend_3:
  phi.f32 $1, .Lfalse_3, $10, .Ltrue_3, $11
  ret.f32 $1
