
  .section .data
test_after:
  .quad .L0

  .text
func_after:
.L0:
  ret

fun_before:
.L1:
  ret


  .section .data
test_before:
  .quad .L1
