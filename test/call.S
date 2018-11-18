  .text

_caller:
  .args        0
  .call        c

  addr.i32     $0, _ret_i32
  call.i32     $1, c, $0

  addr.i64     $3, _ret_i64
  call.i64     $4, c, $3


  addr.f32     $6, _ret_f32
  call.f32     $7, c, $6


  addr.f64     $9, _ret_f64
  call.f64     $10, c, $9

  addr.i32     $2, _arg_i32
  call         c, $2, $1
  addr.i64     $5, _arg_i64
  call         c, $5, $4
  addr.f32     $8, _arg_f32
  call         c, $8, $7
  addr.f64     $11, _arg_f64
  call         c, $11, $10

  ret

_arg_i32:
  .args       1
  ret

_arg_i64:
  .args       1
  ret

_arg_f32:
  .args       1
  ret

_arg_f64:
  .args       1
  ret

_ret_i32:
  imm.i32     $0, 123
  ret.i32     $0

_ret_i64:
  imm.i64     $0, 123
  ret.i64     $0

_ret_f32:
  imm.f32     $0, 0
  ret.f32     $0

_ret_f64:
  imm.f64     $0, 0
  ret.f64     $0
