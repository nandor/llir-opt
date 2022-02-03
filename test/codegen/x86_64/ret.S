# RUN: %opt - -triple x86_64

  .section .text
caml_call_gc:
  .call       c

  get.i64     $3, $ret_addr
  mov.i64     $4, caml_last_return_address
  store.8        [$4], $3

  get.i64     $5, $frame_addr
  mov.i64     $6, caml_bottom_of_stack
  store.8        [$6], $5

  mov.i64     $7, caml_collect
  tcall.c     $7
  .end

  .section .text
caml_alloc1:
  .call       c

.Lalloc1:
  mov.i64     $0, caml_young_ptr
  load.8.i64    $1, [$0]
  mov.i64     $2, 16
  sub.i64     $3, $1, $2

  mov.i64     $4, caml_young_limit
  load.8.i64    $5, [$4]

  cmp.ge.i32  $6, $3, $5
  jf          $6, .Lcollect1

  store.8        [$0], $3
  mov.i64     $7, 8
  add.i64     $8, $3, $7
  ret.i64     $8

.Lcollect1:
  get.i64     $3, $ret_addr
  mov.i64     $4, caml_last_return_address
  store.8        [$4], $3

  get.i64     $5, $frame_addr
  mov.i64     $6, caml_bottom_of_stack
  store.8        [$6], $5

  mov.i64     $99, caml_collect
  call.c      $99
  jump        .Lalloc1
  .end

caml_collect:
  trap
  .end

  .section .data
caml_young_ptr:
  .quad 0
caml_young_limit:
  .quad 0
caml_last_return_address:
  .quad 0
caml_bottom_of_stack:
  .quad 0
