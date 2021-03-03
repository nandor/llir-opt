# RUN: %opt - -triple x86_64


xsaveopt:
  .args i64
  mov.i64       $0, -1
  arg.i64       $1, 0
  x86_xsaveopt  $1, $0
  ret
  .end


xrstor:
  .args i64
  mov.i64       $0, -1
  arg.i64       $1, 0
  x86_xrstor    $1, $0
  ret
  .end
