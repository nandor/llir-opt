  .code
caml_alloc1:
  .call       c
  .args       0
.Lalloc1:
  mov.i64     $0, caml_young_ptr
  ld.8.i64    $1, [$0]
  mov.i64     $2, 16
  sub.i64     $3, $1, $2

  mov.i64     $4, caml_young_limit
  ld.8.i64    $5, [$4]

  cmp.ge.i32  $6, $3, $5
  jf          $6, .Lcollect1

  st.8        [$0], $3
  mov.i64     $7, 8
  add.i64     $8, $3, $7
  ret.i64     $8

.Lcollect1:
  mov.i64     $99, caml_call_gc
  call.c      $99
  jmp         .Lalloc1

  .extern caml_young_ptr
  .extern caml_young_limit
  .extern caml_call_gc
