# RUN: %opt - -triple x86_64 -O0
  .globl  skip_stuff
skip_stuff:
  .noinline
  .call c
  mov.i64 $2, intern_src
  load.i64  $6, [$2]
  mov.i64         $3, 1
.LBB0_1:
  add.i64 $1, $6, $3
  load.i8   $4, [$6]
  mov.i64 $6, $1
  # CHECK: cmpb
  jt      $4, .LBB0_1
.Lfallthrough:
  mov.i64 $5, intern_src
  store      [$5], $1
  ret
  .end
  .end
