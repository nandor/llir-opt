  .code
caller:
  mov.i64 $0, _callee
  call.c  $0
  ret
