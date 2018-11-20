  .text
_caller_va:
  mov.i64     $0, _callee_va
  mov.i32     $1, 0
  invoke.i32  $2, c, $0, $1, .Lerror
  ret.i32     $2
.Lerror:
  mov.i32     $3, 1
  ret.i32     $3


_callee_va:
  .args 1
  mov.i32     $0, 0
  ret.i32     $0
