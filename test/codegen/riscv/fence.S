# RUN: %opt - -triple riscv64 -mcpu sifive-u74

main:
  # CHECK: fence
  riscv_fence
  ret
