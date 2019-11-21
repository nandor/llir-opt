  .code
caml_collect:
  .visibility extern
  .call caml_gc
  .stack_object  0, 120, 8

  frame.i64   $1, 0, 0
  mov.i64     $2, $rax
  st.8        [$1], $2
  frame.i64   $1, 0, 8
  mov.i64     $2, $rbx
  st.8        [$1], $2
  frame.i64   $1, 0, 16
  mov.i64     $2, $rdi
  st.8        [$1], $2
  frame.i64   $1, 0, 24
  mov.i64     $2, $rsi
  st.8        [$1], $2
  frame.i64   $1, 0, 32
  mov.i64     $2, $rdx
  st.8        [$1], $2
  frame.i64   $1, 0, 40
  mov.i64     $2, $rcx
  st.8        [$1], $2
  frame.i64   $1, 0, 48
  mov.i64     $2, $r8
  st.8        [$1], $2
  frame.i64   $1, 0, 56
  mov.i64     $2, $r9
  st.8        [$1], $2
  frame.i64   $1, 0, 64
  mov.i64     $2, $r12
  st.8        [$1], $2
  frame.i64   $1, 0, 72
  mov.i64     $2, $r13
  st.8        [$1], $2
  frame.i64   $1, 0, 80
  mov.i64     $2, $r10
  st.8        [$1], $2
  frame.i64   $1, 0, 88
  mov.i64     $2, $r11
  st.8        [$1], $2
  frame.i64   $1, 0, 96
  mov.i64     $2, $rbp
  st.8        [$1], $2
  frame.i64   $1, 0, 104
  mov.i64     $2, $r14
  st.8        [$1], $2
  frame.i64   $1, 0, 112
  mov.i64     $2, $r15
  st.8        [$1], $2

  mov.i64     $0, caml_gc_regs
  frame.i64   $1, 0, 0
  st.8        [$0], $1

  mov.i64       $2, caml_garbage_collection
  call.c        $2

  frame.i64   $0, 0, 112
  ld.8.i64    $1, [$0]
  set         $r15, $1
  frame.i64   $0, 0, 104
  ld.8.i64    $1, [$0]
  set         $r14, $1
  frame.i64   $0, 0, 96
  ld.8.i64    $1, [$0]
  set         $rbp, $1
  frame.i64   $0, 0, 88
  ld.8.i64    $1, [$0]
  set         $r11, $1
  frame.i64   $0, 0, 80
  ld.8.i64    $1, [$0]
  set         $r10, $1
  frame.i64   $0, 0, 72
  ld.8.i64    $1, [$0]
  set         $r13, $1
  frame.i64   $0, 0, 64
  ld.8.i64    $1, [$0]
  set         $r12, $1
  frame.i64   $0, 0, 56
  ld.8.i64    $1, [$0]
  set         $r9, $1
  frame.i64   $0, 0, 48
  ld.8.i64    $1, [$0]
  set         $r8, $1
  frame.i64   $0, 0, 40
  ld.8.i64    $1, [$0]
  set         $rcx, $1
  frame.i64   $0, 0, 32
  ld.8.i64    $1, [$0]
  set         $rdx, $1
  frame.i64   $0, 0, 24
  ld.8.i64    $1, [$0]
  set         $rsi, $1
  frame.i64   $0, 0, 16
  ld.8.i64    $1, [$0]
  set         $rdi, $1
  frame.i64   $0, 0, 8
  ld.8.i64    $1, [$0]
  set         $rbx, $1
  frame.i64   $0, 0, 0
  ld.8.i64    $1, [$0]
  set         $rax, $1

  ret

  .extern caml_gc_regs
  .extern caml_garbage_collection
