# RUN: %opt - -triple powerpc64le

# CHECK: .quad .Ltmp0-4
# CHECK: .quad .Ltmp1-4
# CHECK: .quad .Ltmp2

indirect_call:
  .args          i64, i64, i64
  .call          caml
  arg.i64        $0, 0
  arg.i64        $1, 1
  arg.i64        $2, 2
  mov.i64        $4, 5
  call.caml.i64  $5, $2, $4, $1, $0  @caml_frame()
  ret.i64        $5
  .end

direct_call_nop:
  .args          i64, i64, i64
  .call          caml
  arg.i64        $0, 0
  arg.i64        $1, 1
  arg.i64        $2, 2
  mov.i64        $4, 5
  mov.i64        $5, f
  call.caml.i64  $6, $5, $2, $4, $1, $0  @caml_frame()
  ret.i64        $6
  .end

direct_call:
  .args          i64, i64, i64
  .call          caml
  arg.i64        $0, 0
  arg.i64        $1, 1
  arg.i64        $2, 2
  mov.i64        $4, 5
  mov.i64        $5, direct_callee
  call.caml.i64  $6, $5, $2, $4, $1, $0  @caml_frame()
  ret.i64        $6
  .end


direct_callee:
  .call         caml
  trap
  .end
