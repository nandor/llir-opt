# RUN: %opt - -triple riscv64 -mcpu sifive-u74

caller:
  .call         caml
  .args         i64
  arg.i64       $0, 0
  mov.i64       $1, callee
  mov.i64       $3, Caml_state
  load.i64        $4, $3
  call.i64.c    $2, $1, $0, $4 @caml_frame
  ret.i64       $2
  .end



callee:
  ret
  .end


  .section .data
Caml_state:
  .quad 0
