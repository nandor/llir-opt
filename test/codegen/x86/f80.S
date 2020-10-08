# ARGS: -O0


# CHECK: ctypes_copy_ldouble:
ctypes_copy_ldouble:
  .visibility          global_default
  .stack_object        0, 16, 16
  .args                f80
  .call                c
  # CHECK: fldt
  # CHECK: fstpt
  arg.f80              $1, 0
  frame.i64            $12, 0, 0
  st.10                [$12], $1
  mov.i64              $2, 1
  mov.i64              $3, 0
  mov.i64              $4, 16
  mov.i64              $5, caml_ldouble_ops$47
  mov.i64              $6, caml_alloc_custom
  call.i64.c           $7, $6, $5, $4, $3, $2
  mov.i64              $8, 8
  add.i64              $9, $7, $8
  mov.i64              $10, memcpy
  frame.i64            $13, 0, 0
  call.i64.c           $11, $10, $9, $13, $4
  # CHECK: retq
  ret                  $7
  .end


ctypes_ldouble_inf:
  .args i64
  .visibility global_default
  .call c
  mov.f64   $1, 0
  log.f64 $2, $1
  neg.f64 $3, $2
  fext.f80  $4, $3
  mov.i64 $5, no_impl
  call.i64.c  $6, $5, $4
  ret
  .end


extend:
  .args i64, i64
  .call c
  .visibility global_default
  arg.i64 $0, 0
  arg.i64 $1, 1
  ld.f64  $2, [$0]
  fext.f80  $3, $2
  st  [$1], $3
  ret
  .end

copy:
  .args i64, i64
  .call c
  .visibility global_default
  arg.i64 $0, 0
  arg.i64 $1, 1
  ld.f80  $2, [$0]
  st  [$1], $2
  ret
  .end
