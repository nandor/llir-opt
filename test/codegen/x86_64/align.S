# RUN: %opt - -triple x86_64
  .section .text
func:
  ret
  .end

  .section .data
  .align 8
  .quad 0
data:
  .quad func
  .quad 0
  .double 12345
