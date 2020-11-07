# RUN: %opt - -triple aarch64

caml_alloc1:
  .call caml_alloc
  .args                   i64, i64

  arg.i64                 $0, 0       # Caml_state
  arg.i64                 $1, 1       # Caml_state->young_ptr

  mov.i64                 $2, 8
  add.i64                 $3, $0, $2
  ld.i64                  $4, [$3]    # Caml_state->young_limit

  mov.i64                 $5, 16
  sub.i64                 $6, $1, $5

  cmp.i8.lt               $7, $6, $4
  jcc                     $7, .Lno_gc_alloc1, .Lgc_alloc1
.Lgc_alloc1:
  mov.i64                 $8, caml_call_gc
  tcall.i64.i64.caml_gc   $8, $0, $6
.Lno_gc_alloc1:
  ret                     $0, $6
  .end
