# RUN: %opt - -triple x86_64
  .section .text
_caller_va:
  mov.i64       $0, _callee_va
  mov.i32       $1, 0
  invoke.i32.c  $2, $0, $1, .Lerror
  ret.i32       $2
.Lerror:
  mov.i32       $3, 1
  ret.i32       $3
  .end


_callee_va:
  .args         i32
  mov.i32       $0, 0
  ret.i32       $0
  .end
