  .text

_caller:
  .args           0
  .call           caml

  mov.i64         $0, _callee
  mov.i64         $1, 1
  mov.i64         $2, 2
  mov.i64         $3, 3
  mov.i64         $4, 4
  mov.i64         $5, 5
  call.i64.caml  $6, $0, $1, $2, $3, $4, $5
  ret.i64         $6

_callee:
  .args           0, i64, i64, i64, i64, i64
  .call           caml
  mov.i64         $0, 1
  ret.i64         $0
