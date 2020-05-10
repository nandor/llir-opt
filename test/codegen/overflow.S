  .text
umulo:
  .args   0, i32, i32

  arg.i32   $0, 0
  arg.i32   $1, 1
  umulo.i32 $2, $0, $1
  ret.i32   $2

uaddo:
  .args   0, i32, i32

  arg.i32   $0, 0
  arg.i32   $1, 1
  uaddo.i32 $2, $0, $1
  ret.i32   $2
