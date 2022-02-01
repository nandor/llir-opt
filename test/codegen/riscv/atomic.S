# RUN: %opt - -triple riscv64 -mfs +a

  .section .data
var:
  .quad 0
