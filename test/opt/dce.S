# ARGS: -passes=dead-code-elim -emit=llir
  .section .text
dce:
  .args     i64, i64, i64

  arg.i64   $0, 0
  arg.i64   $1, 1
  arg.i64   $2, 2
  # CHECK: dce
  # CHECK: arg
  # CHECK: ret
  ret       $2
  .end
