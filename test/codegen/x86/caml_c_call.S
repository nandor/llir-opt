# RUN: %opt - -triple x86_64

caml_callee:
  .call               c
  ret
  .end


caml_caller:
  .call               caml

  mov.i64             $0, caml_callee
  call.i64.c          $1, $0
  ret                 $1
  .end
