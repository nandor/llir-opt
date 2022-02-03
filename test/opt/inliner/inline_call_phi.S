# RUN: %opt - -pass=inliner -emit=llir

callee:
  .call       c
  .args       i64
  .visibility local

  arg.i64     $0, 0
  jump_cond         $0, .Lcallee_true, .Lcallee_false
.Lcallee_true:
  mov.i64     $1, 1
  ret.i64     $1
.Lcallee_false:
  mov.i64     $2, 2
  ret.i64     $2
  .end

caller:
  .call       c
  .args       i64, i64
  .visibility global_default
.Lcaller_entry:
  arg.i64     $0, 0
  mov.i64     $1, 1
  jump_cond         $0, .Lcaller_true, .Lcaller_false
.Lcaller_true:
  mov.i64     $2, callee
  call.i64.c  $3, $2, .Lcaller_false
.Lcaller_false:
  phi.i64     $4, .Lcaller_entry, $1, .Lcaller_true, $3
  ret.i64     $4

