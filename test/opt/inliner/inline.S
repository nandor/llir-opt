# RUN: %opt - -passes=inliner -emit=llir
  .section .text

callee_tcall:
  .args   i64

  mov.i64       $0, tcall
  arg.i64       $1, 0
  tcall.c.i64   $0, $1
  .end

caller_tcall:
  .visibility global_default

  mov.i64    $0, 0
  mov.i64    $1, callee_tcall
  call.i64.c $2, $1, $0
  ret.i64    $2
  .end



callee_void:
  .args   i32

  ret
  .end

caller_void:
  .visibility global_default

  mov.i32    $0, 0
  mov.i64    $1, callee_void
  call.c     $1, $0
  ret
  .end



callee_simple:
  .args   i32

  mov.i64   $0, 100
  ret.i64   $0
  .end

caller_simple:
  .visibility global_default

  mov.i32    $0, 0
  mov.i64    $1, callee_simple
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4
  .end



callee_arg:
  .args   i64

  arg.i64   $0, 0
  ret.i64   $0
  .end

caller_arg:
  .visibility global_default

  mov.i64    $0, 20
  mov.i64    $1, callee_arg
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4
  .end



callee_complex:
  .args  i32

  arg.i32   $0, 0
  jt        $0, .Ltrue_complex
  mov.i64   $1, 100
  ret.i64   $1
.Ltrue_complex:
  mov.i64   $1, 200
  ret.i64   $1
  .end

caller_complex:
  .visibility global_default

  mov.i32    $0, 0
  mov.i64    $1, callee_complex
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4
  .end


callee_merge:
  .args     i32
  arg.i32   $0, 0
  jt        $0, .Ltrue_merge
.Lfalse_merge:
  mov.i64   $1, 100
  jmp       .Lend_merge
.Ltrue_merge:
  mov.i64   $2, 200
.Lend_merge:
  phi.i64   $3, .Lfalse_merge, $1, .Ltrue_merge, $2
  ret.i64   $3
  .end

caller_merge:
  .visibility global_default

  mov.i32    $0, 0
  mov.i64    $1, callee_merge
  call.i64.c $2, $1, $0
  mov.i64    $3, 10
  add.i64    $4, $2, $3
  ret.i64    $4
  .end


callee_trap:
  .args     i64

  trap
  .end

caller_trap:
  .visibility global_default

  mov.i64    $0, 0
  mov.i64    $1, callee_trap
  call.c     $1, $0
  trap
  .end
