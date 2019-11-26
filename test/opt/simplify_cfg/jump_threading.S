


  .code
func:
  .visibility extern
  .args       0, i32

  mov.i64     $0, 100
  arg.i32     $1, 0
  jt          $1, .L1
.Lt:
  mov.i64     $2, 200
  jmp         .L4
.L1:
  jmp         .L2
.L2:
  jmp         .L3
.L3:
  jmp         .L4
.L4:
  phi.i64     $3, .Lt, $2, .L3, $0
  ret.i64     $3
