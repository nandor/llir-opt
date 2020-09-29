  .section .text
live_because_return:
  .visibility extern
  mov.i64                $0, caml_alloc1
  call.i64.caml_alloc    $1, $0
  mov.i64                $3, 1
  st.8                   [$1], $3
  ret.i64                $1

