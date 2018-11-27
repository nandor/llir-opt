  .text
_stack_with_spills:
  .call c
  .stack  80

  mov.i32   $1, 0
  mov.i64   $2, _f

  frame.i64    $22, 72
  call.i32.c    $3, $2, $22, $1

  frame.i64     $23, 64
  call.i32.c    $4, $2, $23, $1
  add.i32       $5, $4, $3

  frame.i64     $24, 56
  call.i32.c    $6, $2, $24, $1
  add.i32       $7, $5, $6

  frame.i64     $25, 48
  call.i32.c    $8, $2, $25, $1
  add.i32       $9, $7, $8

  frame.i64     $26, 40
  call.i32.c    $10, $2, $26, $3
  add.i32       $11, $9, $10

  frame.i64     $27, 32
  call.i32.c    $12, $2, $27, $4
  add.i32       $13, $11, $12

  frame.i64     $28, 24
  call.i32.c    $14, $2, $28, $6
  add.i32       $15, $13, $14

  frame.i64     $29, 16
  call.i32.c    $16, $2, $29, $8
  add.i32       $17, $15, $16

  frame.i64     $30, 8
  call.i32.c    $18, $2, $30, $10
  add.i32       $19, $17, $18

  frame.i64     $31, 0
  call.i32.c    $20, $2, $31, $12
  add.i32       $21, $19, $20

  ret.i32       $21

_f:
  mov.i32 $0, 0
  ret.i32 $0
