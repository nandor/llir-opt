# ARGS: -O0


add_one:
  .args f80
  arg.f80 $0, 0
  mov.f80 $1, 0x41cdcd6500000000
  add.f80 $2, $0, $1
  ret     $2
