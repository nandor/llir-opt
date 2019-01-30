
  .data data
test_after:
  .quad .L0

  .code
func_after:
.L0:
  ret

fun_before:
.L1:
  ret


  .data data
test_before:
  .quad .L1
