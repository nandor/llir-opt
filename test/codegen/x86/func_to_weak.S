# ARGS: -O0 -shared

  .section .text
  .extern f
  .weak f
  .hidden f, ".text"

  .section .text
reference:
  # CHECK: f(%rip)
  mov.i64   $0, f
  ret       $0
