# RUN: %opt - -O0 -shared

  .section .data

  .extern data_local

  .extern data_global_default
  .globl data_global_default

  .extern data_global_hidden
  .globl data_global_hidden
  .hidden data_global_hidden

  .extern data_weak_default
  .weak data_weak_default

  .extern data_weak_hidden
  .weak data_weak_hidden
  .hidden data_weak_hidden

  .section .text
test:
  .noinline
  .call c
  # CHECK: data_global_default@GOTPCREL(%rip)
  # CHECK: data_global_hidden(%rip)
  # CHECK: data_weak_hidden(%rip)
  # CHECK: data_weak_default@GOTPCREL(%rip)
  mov.i64 $1, data_global_hidden
  ld.i32  $2, [$1]
  mov.i64 $3, data_global_default
  ld.i32  $4, [$3]
  add.i32 $5, $4, $2
  mov.i64 $6, data_weak_hidden
  ld.i32  $7, [$6]
  add.i32 $8, $5, $7
  mov.i64 $9, data_weak_default
  ld.i32  $10, [$9]
  add.i32 $11, $8, $10
  ret $11
  .end
