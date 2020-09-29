# ARGS: -mcpu=nehalem
  .section .text
# CHECK: redisAsciiArt
redisAsciiArt:
  .call c
  mov.i64       $0, printf
  mov.i64       $1, L.str
  mov.i64       $2, 2
  mov.i64       $3, 3
  mov.i64       $4, 4
  mov.i64       $5, 5
  mov.i64       $6, 6
  mov.i64       $7, 7
  mov.i64       $8, 8
  mov.i64       $9, 9
  mov.i64       $10, 10
  mov.i64       $11, 11
  # CHECK: pushq
  # CHECK: pushq
  # CHECK: pushq
  # CHECK: pushq
  tcall.i32.1.c $0, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10
  .end

.section .data
L.str:
  .ascii "%d %d %d %d %d %d %d %d %d\n\0"
  .space 1
