# RUN: %opt - -triple x86_64

_caller:
  .call        c

  mov.i64      $1, _callee
  undef.i64    $2
  call.c       $1, $2

  ret
  .end


_callee:
  .args        i64
  ret
  .end
