# RUN: %opt - -triple aarch64

ll_32:
  .args           i64
  arg.i64         $0, 0
  aarch64_ll.i32  $1, $0
  ret             $1
  .end


ll_64:
  .args           i64
  arg.i64         $0, 0
  aarch64_ll.i64  $1, $0
  ret             $1
  .end

sc:
  .args           i64, i32
  arg.i64         $0, 0
  arg.i32         $1, 1
  aarch64_sc.i32  $2, $0, $1
  ret             $2
  .end

sc:
  .args           i64, i64
  arg.i64         $0, 0
  arg.i64         $1, 1
  aarch64_sc.i32  $2, $0, $1
  ret             $2
  .end
