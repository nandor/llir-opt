# RUN: %opt - -passes=dedup-block,simplify-cfg -emit=llir

# CHECK: .LBBentry
# CHECK: mov.i32
# CHECK: ret
main:
  .call       c
  .visibility global_default
  .args       i8
  arg.i8      $0, 0
  jt          $0, .Ltrue
.Lfalse:
  mov.i32     $1, 1
  ret         $1
.Ltrue:
  mov.i32     $1, 1
  ret         $1
  .end
