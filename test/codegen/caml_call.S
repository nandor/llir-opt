
caller:
  .call         caml
  .args         i64
  arg.i64       $0, 0
  mov.i64       $1, callee
  call.i64.c    $2, $1, $0  @caml_frame
  ret.i64       $2



callee:
  ret
