  .text
caml_call_gc:
  .call       c
  .args       0

  mov.i64     $3, $ret_addr
  mov.i64     $4, caml_last_return_address
  st.8        [$4], $3

  mov.i64     $5, $frame_addr
  mov.i64     $6, caml_bottom_of_stack
  st.8        [$6], $5

  mov.i64     $7, caml_collect
  tcall.c     $7

  .text
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
  mov.i64     $3, $ret_addr
  mov.i64     $4, caml_last_return_address
  st.8        [$4], $3

  mov.i64     $5, $frame_addr
  mov.i64     $6, caml_bottom_of_stack
  st.8        [$6], $5

  mov.i64     $99, caml_collect
  call.c      $99
  jmp         .Lalloc1

caml_collect:
  trap

  .section .data
caml_young_ptr:
  .quad 0
caml_young_limit:
  .quad 0
caml_last_return_address:
  .quad 0
caml_bottom_of_stack:
  .quad 0
