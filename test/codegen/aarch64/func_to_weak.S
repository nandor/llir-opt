# RUN: %opt - -O0 -shared -triple aarch64

  .section .text
  .extern f, ".text"
  .weak f
  .hidden f

  .section .text
reference:
  # CHECK: x0, f
  mov.i64   $0, f
  ret       $0
