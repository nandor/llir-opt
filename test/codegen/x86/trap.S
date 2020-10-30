# RUN: %opt - -triple x86_64
  .section .text
_trap:
  .call        c
  trap
  .end
