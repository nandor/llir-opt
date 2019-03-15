  .code
_reg_frame:
  .call             caml
  .args             0, i64, i64, i64, i64
  arg.i64           $0, 0
  arg.i64           $1, 1
  arg.i64           $2, 2
  arg.i64           $3, 3
  mov.i64           $4, _callee_c
  mov.i64           $5, _caml_c_call
  call.i64.caml_ext $0, $5, $4, $0, $1, $2, $3 @caml_call_frame
  ret.i64           $0

_stack_frame:
  .call             caml
  .stack            0
  .args             0, i64

  arg.i64           $0, 0

  mov.i64           $2, _callee_val
  call.i64.caml    $3, $2

  mov.i64           $4, _callee_frame
  call.i64.caml    $5, $4, $0                @caml_call_frame

  add.i64           $6, $3, $5
  ret.i64           $6

_raise_frame:
  .call             caml
  .stack            0
  .args             0, i64
  arg.i64           $0, 0
  mov.i64           $1, _caml_raise_exn
  tcall.c           $1, $0                    @caml_raise_frame

_callee_val:
  .call             caml
  trap

_callee_frame:
  .call             caml
  .args             0, i64
  trap

_caml_raise_exn:
  .args             0, i64
  trap

_caml_c_call:
  .args             0, i64, i64, i64, i64
  .call             caml
  trap

_callee_c:
  .args             0, i64, i64, i64, i64
  .call             c
  trap
