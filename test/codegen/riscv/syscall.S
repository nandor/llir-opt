# RUN: %opt - -triple riscv64 -mcpu sifive-u74

syscall:
  .visibility global_default
  .args       i64, i64, i64

  arg.i64     $0, 0
  arg.i64     $1, 1
  arg.i64     $2, 2

  mov.i64     $3, 0
  # CHECK: ecall
  syscall.i64 $5, $3, $0, $1, $2
  ret         $5
  .end
