_switch:
  .args i32
  .call c

  arg.i32 $1, 0
  mov.i32 $5, 14
  cmp.ugt.i32 $6, $1, $5
  jt  $6, .Ldefault
  zext.i64  $2, $1
  switch  $2, .LBB0_2, .LBB0_3, .LBB0_4, .LBB0_5, .LBB0_6, .Ldefault, .Ldefault, .Ldefault, .Ldefault, .Ldefault, .LBB0_7, .LBB0_8, .LBB0_9, .LBB0_10, .LBB0_11
.LBB0_2:
  mov.i64  $16, _f10
  call.c   $16
  ret
.LBB0_3:
  mov.i64  $15, _f11
  call.c   $15
  ret
.LBB0_4:
  mov.i64  $14, _f12
  call.c   $14
  ret
.LBB0_5:
  mov.i64  $13, _f13
  call.c   $13
  ret
.LBB0_6:
  mov.i64  $12, _f14
  call.c   $12
  ret
.Ldefault:
  mov.i64  $17, _fdefault
  call.c   $17
  ret
.LBB0_7:
  mov.i64  $11, _f20
  call.c   $11
  ret
.LBB0_8:
  mov.i64  $10, _f21
  call.c   $10
  ret
.LBB0_9:
  mov.i64  $9, _f22
  call.c   $9
  ret
.LBB0_10:
  mov.i64  $8, _f23
  call.c   $8
  ret
.LBB0_11:
  mov.i64  $7, _f24
  call.c   $7
  ret
  .end

_f10:
  ret
  .end

_f11:
  ret
  .end

_f12:
  ret
  .end

_f13:
  ret
  .end

_f14:
  ret
  .end

_fdefault:
  ret
  .end

_f20:
  ret
  .end

_f21:
  ret
  .end

_f22:
  ret
  .end

_f23:
  ret
  .end

_f24:
  ret
  .end

