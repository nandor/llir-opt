# RUN: %opt - -passes=inliner,simplify-cfg -emit=llir


trap_callee:
  .call c
  trap
  .end

# CHECK: trap_caller_1
# CHECK: trap
trap_caller_1:
  .args       i8
  .visibility global_default
.Lentry:
  arg.i8      $0, 0
  mov.i64     $5, 100
  jt          $0, .Ltrue
.Lfalse:
  mov.i64     $1, trap_callee
  call.c.i64  $2, $1, .Lcont
.Lcont:
  jmp         .Ltrue
.Ltrue:
  phi.i64     $3, .Lentry, $5, .Lcont, $2
  ret.i64     $3
  .end

# CHECK: trap_caller_2
# CHECK: trap
trap_caller_2:
  .args       i8
  .visibility global_default
.Lentry_2:
  arg.i8      $0, 0
  mov.i64     $5, 100
  jt          $0, .Ltrue_2
.Lfalse_2:
  mov.i64     $1, trap_callee
  call.c.i64  $2, $1, .Ltrue_2
.Ltrue_2:
  phi.i64     $3, .Lentry_2, $5, .Lfalse_2, $2
  ret.i64     $3
  .end

annot_callee:
  .call c
  mov.i64 $0, 0
  ret     $0
  .end

# CHECK: annot_caller
# CHECK: call
annot_caller:
  .visibility   global_default
  mov.i64       $0, annot_callee
  call.caml.v64 $1, $0
  ret           $1
  .end
