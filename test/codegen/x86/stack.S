  .section .text
_stack_with_spills:
  .call c
  .stack_object   0, 8, 8
  .stack_object   1, 8, 8
  .stack_object   2, 8, 8
  .stack_object   3, 8, 8
  .stack_object   4, 8, 8
  .stack_object   5, 8, 8
  .stack_object   6, 8, 8
  .stack_object   7, 8, 8
  .stack_object   8, 8, 8
  .stack_object   9, 8, 8

  mov.i32   $1, 0
  mov.i64   $2, _f

  frame.i64     $22, 9, 0
  call.i32.c    $3, $2, $22, $1

  frame.i64     $23, 8, 0
  call.i32.c    $4, $2, $23, $1
  add.i32       $5, $4, $3

  frame.i64     $24, 7, 0
  call.i32.c    $6, $2, $24, $1
  add.i32       $7, $5, $6

  frame.i64     $25, 6, 0
  call.i32.c    $8, $2, $25, $1
  add.i32       $9, $7, $8

  frame.i64     $26, 5, 0
  call.i32.c    $10, $2, $26, $3
  add.i32       $11, $9, $10

  frame.i64     $27, 4, 0
  call.i32.c    $12, $2, $27, $4
  add.i32       $13, $11, $12

  frame.i64     $28, 3, 0
  call.i32.c    $14, $2, $28, $6
  add.i32       $15, $13, $14

  frame.i64     $29, 2, 0
  call.i32.c    $16, $2, $29, $8
  add.i32       $17, $15, $16

  frame.i64     $30, 1, 0
  call.i32.c    $18, $2, $30, $10
  add.i32       $19, $17, $18

  frame.i64     $31, 0, 0
  call.i32.c    $20, $2, $31, $12
  add.i32       $21, $19, $20

  ret.i32       $21
  .end

_f:
  mov.i32 $0, 0
  ret.i32 $0
  .end
