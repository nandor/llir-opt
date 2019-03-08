  .code
caml_collect:
  .call c
  .stack 104

  frame.i64   $0, 0
  mov.i64     $1, $rax
  st.8        [$0], $1
  frame.i64   $0, 8
  mov.i64     $1, $rbx
  st.8        [$0], $1
  frame.i64   $0, 16
  mov.i64     $1, $rdi
  st.8        [$0], $1
  frame.i64   $0, 24
  mov.i64     $1, $rsi
  st.8        [$0], $1
  frame.i64   $0, 32
  mov.i64     $1, $rdx
  st.8        [$0], $1
  frame.i64   $0, 40
  mov.i64     $1, $rcx
  st.8        [$0], $1
  frame.i64   $0, 48
  mov.i64     $1,  $r8
  st.8        [$0],$1
  frame.i64   $0, 56
  mov.i64     $1,  $r9
  st.8        [$0],$1
  frame.i64   $0, 64
  mov.i64     $1, $r12
  st.8        [$0], $1
  frame.i64   $0, 72
  mov.i64     $1, $r13
  st.8        [$0], $1
  frame.i64   $0, 80
  mov.i64     $1, $r10
  st.8        [$0], $1
  frame.i64   $0, 88
  mov.i64     $1, $r11
  st.8        [$0], $1
  frame.i64   $0, 96
  mov.i64     $1, $rbp
  st.8        [$0], $1

  mov.i64     $0, caml_gc_regs
  frame.i64   $1, 0
  st.8        [$0], $1

  mov.i64     $2, caml_garbage_collection
  call.c      $2

  frame.i64   $0, 96
  ld.8.i64    $1, [$0]
  set         $rbp, $1
  frame.i64   $0, 88
  ld.8.i64    $1, [$0]
  set         $r11, $1
  frame.i64   $0, 80
  ld.8.i64    $1, [$0]
  set         $r10, $1
  frame.i64   $0, 72
  ld.8.i64    $1, [$0]
  set         $r13, $1
  frame.i64   $0, 64
  ld.8.i64    $1, [$0]
  set         $r12, $1
  frame.i64   $0, 56
  ld.8.i64    $1, [$0]
  set         $r9, $1
  frame.i64   $0, 48
  ld.8.i64    $1, [$0]
  set         $r8, $1
  frame.i64   $0, 40
  ld.8.i64    $1, [$0]
  set         $rcx, $1
  frame.i64   $0, 32
  ld.8.i64    $1, [$0]
  set         $rdx, $1
  frame.i64   $0, 24
  ld.8.i64    $1, [$0]
  set         $rsi, $1
  frame.i64   $0, 16
  ld.8.i64    $1, [$0]
  set         $rdi, $1
  frame.i64   $0, 8
  ld.8.i64    $1, [$0]
  set         $rbx, $1
  frame.i64   $0, 0
  ld.8.i64    $1, [$0]
  set         $rax, $1

  ret

  .extern caml_gc_regs
  .extern caml_garbage_collection
