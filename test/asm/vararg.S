  .text
_va_call:
  .args        0
  .call        c

  mov.i64 $8, _va_callee
  mov.i64 $7, 5
  mov.f64 $6, 0
  mov.i32 $5, 1
  mov.f64 $4, 4611686018427387904
  mov.i64 $3, 1

  call.f64.1.c  $9, $8, $7, $6, $5, $4, $3
  trunc.i64 $10, $9
  ret.i32 $10

_va_tcall:
  .args        0
  .call        c

  mov.i64 $8, _va_callee
  mov.i64 $7, 5
  mov.f64 $6, 0
  mov.i32 $5, 1
  mov.f64 $4, 4611686018427387904
  mov.i64 $3, 1

  tcall.1.c $8, $7, $6, $5, $4, $3


_va_callee:
  .args        0, i64
  ret


_va_forward:
  .stack    32
  .args     1, i64, i64
  .call c

  frame.i64 $1, 0
  vastart   $1
  mov.i64   $2, _va_forward_callee
  call.c    $2, $1
  ret

_va_forward_callee:
  .args     0, i64
  .call c
  ret
