# RUN: %opt - -triple x86_64 -O0


call_multiple:
  .call c

  mov.i64         $0, return_multiple
  call.c.i64.i64  $1, $2, $0
  add.i64         $3, $1, $2
  ret             $3
  .end

return_multiple:
  .call   caml

  mov.i64 $0, 0
  mov.i64 $1, 1
  ret     $0, $1
  .end
