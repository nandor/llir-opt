_very_long_function:
  .call             caml
  .args             0, i64
  arg.i64           $1001, 0
  mov.i64           $0, $1001
  mov.i64           $0, _camlTest__const_float_377
  mov.i64           $1, _camlTest__const_float_378
  mov.i64           $2, _camlTest__const_float_379
  mov.i64           $3, _camlTest__const_float_380
  mov.i64           $5, _simd_make
  mov.i64           $6, _caml_c_call
  call.i64.caml_ext $0, $6, $5, $3, $2, $1, $0 @caml_frame
  mov.i64           $1, _camlTest__Pfloatcomp_arg_386
  mov.i64           $2, _camlTest__const_float_382
  mov.i64           $3, _camlTest__Pfloatcomp_arg_385
  mov.i64           $4, _camlTest__const_float_384
  mov.i64           $7, _simd_make
  mov.i64           $8, _caml_c_call
  call.i64.caml_ext $1, $8, $7, $4, $3, $2, $1 @caml_frame
  mov.i64           $9, _simd_add
  mov.i64           $10, _caml_c_call
  call.i64.caml_ext $0, $10, $9, $0, $1 @caml_frame
  mov.i64           $1, 1
  mov.i64           $11, _simd_get
  mov.i64           $12, _caml_c_call
  call.i64.caml_ext $1, $12, $11, $0, $1 @caml_frame
  ld.8.f64          $1, [$1]
  mov.f64           $2, 0x4018000000000000
  cmp.une.i32       $13, $1, $2
  jt                $13, .L118.15
  mov.i64           $1, 1
  jmp               .L117.15
.L118.15:
  mov.i64           $0, _camlTest__Pmakeblock_214
  mov.i64           $2000, $0
  mov.i64           $14, _caml_raise_exn
  tcall.c           $14, $2000 @caml_frame
.L117.15:
  mov.i64           $1, 3
  mov.i64           $15, _simd_get
  mov.i64           $16, _caml_c_call
  call.i64.caml_ext $1, $16, $15, $0, $1 @caml_frame
  ld.8.f64          $1, [$1]
  mov.f64           $2, 0x4020000000000000
  cmp.une.i32       $17, $1, $2
  jt                $17, .L116.15
  mov.i64           $1, 1
  jmp               .L115.15
.L116.15:
  mov.i64           $0, _camlTest__Pmakeblock_215
  mov.i64           $2000, $0
  mov.i64           $18, _caml_raise_exn
  tcall.c           $18, $2000 @caml_frame
.L115.15:
  mov.i64           $1, 5
  mov.i64           $19, _simd_get
  mov.i64           $20, _caml_c_call
  call.i64.caml_ext $1, $20, $19, $0, $1 @caml_frame
  ld.8.f64          $1, [$1]
  mov.f64           $2, 0x4024000000000000
  cmp.une.i32       $21, $1, $2
  jt                $21, .L114.15
  mov.i64           $1, 1
  jmp               .L113.15
.L114.15:
  mov.i64           $0, _camlTest__Pmakeblock_216
  mov.i64           $22, _caml_raise_exn
  tcall.c           $22, $0 @caml_frame
.L113.15:
  mov.i64           $1, 7
  mov.i64           $23, _simd_get
  mov.i64           $24, _caml_c_call
  call.i64.caml_ext $0, $24, $23, $0, $1 @caml_frame
  ld.8.f64          $0, [$0]
  mov.f64           $1, 0x4028000000000000
  cmp.une.i32       $25, $0, $1
  jt                $25, .L112.15
  mov.i64           $0, 1
  ret.i64           $0
.L112.15:
  mov.i64           $0, _camlTest__Pmakeblock_217
  mov.i64           $2000, $0
  mov.i64           $26, _caml_raise_exn
  tcall.c           $26, $2000 @caml_frame

.extern _camlTest__const_float_377
.extern _camlTest__const_float_378
.extern _camlTest__const_float_379
.extern _camlTest__const_float_380
.extern _camlTest__const_float_382
.extern _camlTest__const_float_384
.extern _camlTest__Pfloatcomp_arg_385
.extern _camlTest__Pfloatcomp_arg_386
.extern _camlTest__Pmakeblock_214
.extern _camlTest__Pmakeblock_215
.extern _camlTest__Pmakeblock_216
.extern _camlTest__Pmakeblock_217
.extern _simd_make
.extern _simd_add
.extern _simd_get
.extern _caml_c_call
.extern _caml_raise_exn
