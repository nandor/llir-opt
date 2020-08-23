
test:
  .visibility          hidden
  .stack_object        0, 16, 8
  .noinline
  .args                i64
  .call                c
  arg.i64              $10, 0
  mov.i64              $1, 0
  mov.i64              $2, gettimeofday
  frame.i64            $16, 0, 0
  call.i32.c           $3, $2, $16, $1
  mov.i64              $4, 1000000
  frame.i64            $17, 0, 0
  ld.8.i64             $5, [$17]
  mul.i64              $6, $5, $4
  mov.i64              $7, 8
  frame.i64            $18, 0, 0
  add.i64              $8, $18, $7
  ld.8.i64             $9, [$8]
  sub.i64              $11, $9, $10
  add.i64              $12, $11, $6
  sext.f32             $13, $12
  mov.f32              $14, 4696837146684686336
  sdiv.f32             $15, $13, $14
  ret                  $15
