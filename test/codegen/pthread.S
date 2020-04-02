  .extern pthread_create

  .code
thread_func:
  .visibility hidden
  .call       c

  mov.i64     $0, 0
  ret.i64     $0

spawn_thread:
  .visibility   extern
  .call         caml
  .stack_object 0, 8, 8
  .stack_object 1, 8, 8
  .args         0, i64, i64

  mov.i64       $80, thread_func_closure
  mov.i64       $94, caml_thread_start
  frame.i64     $92, 0, 0
  frame.i64     $93, 1, 0
  mov.i64       $90, pthread_create
  call.4.c.i32  $91, $90, $92, $93, $94, $80
  ret.i64       $91


caml_thread_start:
  .visibility   hidden
  .call         c
  .args         0, i64

  arg.i64       $0, 0
  ld.8.i64      $1, [$0]
  call.c        [$1]
  ret


  .data         caml
thread_func_closure:
  .quad         thread_func
