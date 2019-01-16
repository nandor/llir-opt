  .text

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



#callee_complex:
#  .args   0, i32
#
#  arg.i32   $0, 0
#  jt        $0, .Ltrue
#  mov.i64   $1, 100
#  ret.i64   $1
#.Ltrue:
#  mov.i64   $1, 200
#  ret.i64   $1
#
#
#caller_complex:
#  .args   0
#
#  mov.i32    $0, 0
#  mov.i64    $1, callee_complex
#  call.i64.c $2, $1, $0
#  mov.i64    $3, 10
#  add.i64    $4, $2, $3
#  ret.i64    $4
