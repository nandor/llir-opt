# RUN: %opt - -emit=llir

path_join:
  .stack_object 0, 8, 8
  .stack_object 1, 8, 8
  .args         i32

  frame.i64     $0, 0, 0

  frame.i64     $1, 1, 0
  mov.i64       $2, 0
  store            $1, $2

  arg.i32       $3, 0
  jt.i32        $3, .Ltrue
.Lfalse:
  mov.i64       $4, 0
  store            $0, $4
  jump          .Lend
.Ltrue:
  mov.i64       $5, 0
  store            $0, $5
.Lend:
  load.i64        $6, $0
  load.i64        $7, $1
  add.i64       $8, $6, $7
  ret.i64       $8
  .end



