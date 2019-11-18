  .code
caml_allocN:
  .call         caml_alloc
  .args         0, i64
.LallocN:
  mov.i64       $0, caml_young_ptr
  ld.8.i64      $1, [$0]
  arg.i64       $2, 0
  sub.i64       $3, $1, $2

  mov.i64       $4, caml_young_limit
  ld.8.i64      $5, [$4]

  cmp.ge.i32    $6, $3, $5
  jf            $6, .LcollectN

  st.8          [$0], $3
  mov.i64       $7, 8
  add.i64       $8, $3, $7
  ret.i64       $8

.LcollectN:
  mov.i64       $3, $ret_addr
  mov.i64       $4, caml_last_return_address
  st.8          [$4], $3

  mov.i64       $5, $frame_addr
  mov.i64       $6, caml_bottom_of_stack
  st.8          [$6], $5

  mov.i64       $99, caml_collect
  call.caml_gc  $99
  jmp           .LallocN

  .data data
caml_young_ptr:
  .quad 0
caml_young_limit:
  .quad 0
caml_last_return_address:
  .quad 0
caml_bottom_of_stack:
  .quad 0


  .extern caml_collect
