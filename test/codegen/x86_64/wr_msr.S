# RUN: %opt - -triple x86_64


# CHECK: wrmsr
test_wrmsr:
  mov.i32    $0, 0xc0000100
  mov.i32    $1, 0x100
  mov.i32    $2, 0x200
  x86_wr_msr $0, $1, $2
  ret
