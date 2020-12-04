# RUN: %opt - -triple x86_64


call_offset:
  mov.i64       $0, test + 128
  call.xen.i64  $1, $0
  ret           $1
  .end
