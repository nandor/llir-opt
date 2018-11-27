
_caller:
  .args        0
  .call        c

  mov.i64      $1, _callee
  undef.i64    $2
  call.c       $1, $2

  ret


_callee:
  .args        0, i64
  ret
