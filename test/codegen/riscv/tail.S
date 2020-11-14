# RUN: %opt - -triple riscv64 -mcpu sifive-u74

aio_read:
  .visibility global_default
  .call c
  .args i64
.LBBentry1:
  .visibility local
  arg.i64 $0, 0
  mov.i32 $1, 0
  mov.i64 $2, submit
  tcall.2.c.i32 $2, $0, $1
  .end
