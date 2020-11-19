# RUN: %opt - -triple powerpc64le


test_sync:
  # CHECK: sync
  ppc_sync
  ret
  .end


test_isync:
  # CHECK: isync
  ppc_isync
  ret
  .end


test_ll_i32:
  .args i64
  # CHECK: lwarx
  arg.i64     $0, 0
  ppc_ll.i32  $1, $0
  ret         $1
  .end

test_ll_i64:
  .args i64
  # CHECK: ldarx
  arg.i64     $0, 0
  ppc_ll.i64  $1, $0
  ret         $1
  .end

test_sc_i32:
  .args i64, i32
  # CHECK: stwcx.
  arg.i64     $0, 0
  arg.i32     $1, 1
  ppc_sc.i32  $2, $0, $1
  ret         $2
  .end

test_sc_i64:
  .args i64, i64
  # CHECK: stdcx.
  arg.i64     $0, 0
  arg.i64     $1, 1
  ppc_sc.i32  $2, $0, $1
  ret         $2
  .end
