# ARGS: -passes=inliner,simplify-cfg -emit=llir


trap_callee:
  .call c
  trap

# CHECK: trap_caller
# CHECK: trap
trap_caller:
  .args       i8
.Lentry:
  arg.i8      $0, 0
  mov.i64     $5, 100
  jt          $0, .Ltrue
.Lfalse:
  mov.i64     $1, trap_callee
  call.c.i64  $2, $1
  jmp         .Ltrue
.Ltrue:
  phi.i64     $3, .Lentry, $5, .Lfalse, $2
  ret.i64     $3



annot_callee:
  .call c
  mov.i64 $0, 0
  ret     $0

# CHECK: annot_caller
# CHECK: @caml_value
annot_caller:
  mov.i64       $0, annot_callee
  call.caml.i64 $1, $0 @caml_value
  ret.i64       $1
