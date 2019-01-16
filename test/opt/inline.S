  .text


callee_void:
  .args   0, i32

  ret

caller_void:
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
  .args   0

  mov.i64    $0, 20
  mov.i64    $1, callee_arg
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4

