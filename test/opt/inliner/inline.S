  .code

  .extern tcall

callee_tcall:
  .args   0, i64

  mov.i64       $0, tcall
  arg.i64       $1, 0
  tcall.c.i64   $0, $1

caller_tcall:
  .visibility extern
  .args   0

  mov.i64    $0, 0
  mov.i64    $1, callee_tcall
  call.i64.c $2, $1, $0
  ret.i64    $2



callee_void:
  .args   0, i32

  ret

caller_void:
  .visibility extern
  .args   0

  mov.i32    $0, 0
  mov.i64    $1, callee_void
  call.c     $1, $0
  ret



callee_simple:
  .args   0, i32

  mov.i64   $0, 100
  ret.i64   $0

caller_simple:
  .visibility extern
  .args   0

  mov.i32    $0, 0
  mov.i64    $1, callee_simple
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4



callee_arg:
  .args   0, i64

  arg.i64   $0, 0
  ret.i64   $0

caller_arg:
  .visibility extern
  .args   0

  mov.i64    $0, 20
  mov.i64    $1, callee_arg
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4



callee_complex:
  .args   0, i32

  arg.i32   $0, 0
  jt        $0, .Ltrue_complex
  mov.i64   $1, 100
  ret.i64   $1
.Ltrue_complex:
  mov.i64   $1, 200
  ret.i64   $1

caller_complex:
  .visibility extern
  .args   0

  mov.i32    $0, 0
  mov.i64    $1, callee_complex
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4


callee_merge:
  .args   0, i32
  arg.i32   $0, 0
  jt        $0, .Ltrue_merge
.Lfalse_merge:
  mov.i64   $1, 100
  jmp       .Lend_merge
.Ltrue_merge:
  mov.i64   $2, 200
.Lend_merge:
  phi.i64   $3, .Lfalse_merge, $1, .Ltrue_merge, $2
  ret.i64   $3

caller_merge:
  .args   0
  .visibility extern

  mov.i32    $0, 0
  mov.i64    $1, callee_merge
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4


callee_trap:
  .args   0, i64

  trap

caller_trap:
  .args   0
  .visibility extern

  mov.i64    $0, 0
  mov.i64    $1, callee_trap
  call.c     $1, $0
  trap
