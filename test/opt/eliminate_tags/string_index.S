# RUN: %opt - -pass=eliminate-tags -emit=llir -triple=x86_64

  .align  1
to_optimise:
  .visibility local
  .call caml
  .args i64, i64, v64, v64, v64, v64
.Lentry:
  .visibility local
  arg.i64     $0, 0
  arg.i64     $1, 1
  arg.v64     $2, 2
  arg.v64     $3, 3
  arg.v64     $4, 4
  arg.v64     $5, 5
  jmp         .Lstart
.Lstart:
  phi.v64     $6, .Lentry, $4, .Lpred, $22
  cmp.i8.lt   $7, $6, $3
  jcc         $7, .Lbody, .Lcheck
.Lcheck:
  mov.i64     $8, .Lexception + 8
  mov.i64     $9, 232
  add.i64     $10, $0, $9
  mov.i64     $11, 0
  st          $10, $11
  mov.i64     $12, caml_raise_exn
  call.caml   $12, $0, $1, $8, .Lend @caml_frame(() (((2024409346867202 "string.ml" "Stdlib__string.index_rec"))))
.Lend:
  trap
.Lbody:
  mov.i64     $13, 1
  sra.i64     $14, $6, $13
  add.i64     $15, $2, $14
  ld.i8       $16, $15
  zext.i64    $17, $16
  sll.i64     $18, $17, $13
  add.i64     $19, $18, $13
  cmp.i8.ne   $20, $19, $5
  jcc         $20, .Lpred, .Lexit
.Lexit:
  ret         $0, $1, $6
.Lpred:
  mov.i64     $21, 2
  add.i64     $22, $6, $21
  jmp         .Lstart
  .end

  .align 1
entry:
  .visibility global_default
  .call caml
  .args i64, i64, v64, v64, v64, v64
  arg.i64         $0, 0
  arg.i64         $1, 1
  arg.v64         $2, 2
  mov.i64         $3, 3
  arg.v64         $4, 4
  arg.v64         $5, 5
  mov.i64         $20, to_optimise
  tcall.caml.i64  $20, $0, $1, $2, $3, $4, $5
