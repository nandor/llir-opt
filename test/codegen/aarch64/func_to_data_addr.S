# ARGS: -O0 -shared -triple aarch64

  .section .data
data_local:
  .visibility local
  .quad 0
  .end

  .section .data
data_global_hidden:
  .visibility global_hidden
  .quad 0
  .end

  .section .data
data_global_default:
  .visibility global_default
  .quad 0
  .end

  .section .data
data_weak_hidden:
  .visibility weak_hidden
  .quad 0
  .end

  .section .data
data_weak_default:
  .visibility weak_default
  .quad 0
  .end

  .section .text
test:
  .noinline
  .call c
  # CHECK: :got:data_global_default
  # CHECK: :got:data_weak_default
  # CHECK: data_global_hidden
  # CHECK: data_weak_hidden
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
