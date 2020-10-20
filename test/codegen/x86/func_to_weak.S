# ARGS: -O0 -shared

  .section .text
  .extern f, ".text"
  .weak f
  .hidden f

  .section .text
reference:
  # CHECK: f(%rip)
  mov.i64   $0, f
  ret       $0
