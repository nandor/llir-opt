# RUN: %opt - -triple i386

  .section .text
add_32:
  .args   i32, i32

  arg.i32 $0, 0
  arg.i32 $1, 1
  add.i32 $2, $0, $1
  ret.i32 $2
  .end

add_16:
  .args   i16, i16

  arg.i16 $0, 0
  arg.i16 $1, 1
  add.i16 $2, $0, $1
  ret.i16 $2
  .end
