  .text

_caller:
  .args        0, 0
  .call        c

  mov.i64      $0, _callee
  mov.i32      $1, 0
  mov.i32      $2, 1
  mov.i32      $3, 2
  mov.i32      $4, 3
  mov.i32      $5, 4

  tcall.c      $0, $1, $2, $3, $4, $5

_callee:
  .args       5, 0
  ret
