# ARGS: -passes=dedup-block,simplify-cfg -emit=llir

# CHECK: .LBBentry1:
# CHECK: mov.i32
# CHECK: ret
main:
  .call       c
  .visibility extern
  .args       0, i8
  arg.i8      $0, 0
  jt          $0, .Ltrue
.Lfalse:
  mov.i32     $1, 1
  ret         $1
.Ltrue:
  mov.i32     $1, 1
  ret         $1
