caml_apply11:
  .call        caml
  .args        i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64

  mov.i64          $100, sink

  arg.i64          $0, 0    # rax
  arg.i64          $1, 1    # rbx
  arg.i64          $2, 2    # rdi
  arg.i64          $3, 3    # rsi
  arg.i64          $4, 4    # rdx
  arg.i64          $5, 5    # rcx
  arg.i64          $6, 6    # r8
  arg.i64          $7, 7    # r9
  arg.i64          $8, 8    # r12
  arg.i64          $9, 9    # r13
  arg.i64          $10, 10  # stk0
  arg.i64          $11, 11  # stk1
  arg.i64          $12, 12  # stk2
  arg.i64          $13, 13  # stk3

  call.i64.caml  $100, $10
  call.i64.caml  $100, $11
  call.i64.caml  $100, $12
  call.i64.caml  $100, $13 @caml_frame

  ret
