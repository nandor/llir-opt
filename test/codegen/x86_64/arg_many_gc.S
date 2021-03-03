# RUN: %opt - -triple x86_64


func_with_many_args:
  .call           caml
  .args           v64, v64, v64, v64, v64, v64, v64, v64, v64, v64, v64, v64, v64, v64, v64


  arg.v64         $1, 0
  arg.v64         $2, 1
  arg.v64         $3, 2
  arg.v64         $4, 3
  arg.v64         $5, 4
  arg.v64         $6, 5
  arg.v64         $7, 6
  arg.v64         $8, 7
  arg.v64         $9, 8
  arg.v64         $10, 9
  arg.v64         $11, 10
  arg.v64         $12, 11
  arg.v64         $13, 12
  arg.v64         $14, 13
  arg.v64         $15, 14

  mov.i64         $0, callee
  call.caml       $0 @caml_frame

  mov.i64         $16, sink
  call.caml       $16, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15

  ret
