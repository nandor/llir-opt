# RUN: %opt - -triple x86_64

_arg_in_branch:
  .args        i64, i32, i32
  .call        c
  arg.i32      $3, 1
  jf           $3, .Lfalse
  arg.i32      $5, 2
  ret.i64      $5
.Lfalse:
  arg.i64      $1, 0
  ret.i64      $1
