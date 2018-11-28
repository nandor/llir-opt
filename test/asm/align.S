  .text
func:
  ret
  .data
  .align 8
  .quad 0
data:
  .quad func
  .quad 0
  .double 12345
