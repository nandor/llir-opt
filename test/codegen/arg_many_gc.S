

func_with_many_args:
  .call           caml
  .args           0, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64


  arg.i64         $1, 0 @caml_value
  arg.i64         $2, 1 @caml_value
  arg.i64         $3, 2 @caml_value
  arg.i64         $4, 3 @caml_value
  arg.i64         $5, 4 @caml_value
  arg.i64         $6, 5 @caml_value
  arg.i64         $7, 6 @caml_value
  arg.i64         $8, 7 @caml_value
  arg.i64         $9, 8 @caml_value
  arg.i64         $10, 9 @caml_value
  arg.i64         $11, 10 @caml_value
  arg.i64         $12, 11 @caml_value
  arg.i64         $13, 12 @caml_value
  arg.i64         $14, 13 @caml_value
  arg.i64         $15, 14 @caml_value

  mov.i64         $0, callee
  call.caml       $0 @caml_frame

  mov.i64         $16, sink
  call.caml       $16, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15

  ret
