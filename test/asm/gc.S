  .text
_call_frame:
  .call             ocaml
  .stack            0
  .args             0, i64

  arg.i64           $0, 0

  mov.i64           $2, _callee_val
  call.i64.ocaml    $3, $2

  mov.i64           $4, _callee_frame
  call.i64.ocaml    $5, $4, $0                @caml_call_frame

  add.i64           $6, $3, $5
  ret.i64           $6

_raise_frame:
  .call             ocaml
  .stack            0
  .args             0, i64
  arg.i64           $0, 0
  mov.i64           $1, _caml_raise_exn
  tcall.c           $1, $0                    @caml_raise_frame

_callee_val:
  .call             ocaml
  trap

_callee_frame:
  .call             ocaml
  .args             0, i64
  trap

_caml_raise_exn:
  .args             0, i64
  trap
