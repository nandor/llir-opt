# RUN: %opt - -emit=llir

  .section .text
caml_request_major_slice:
  .visibility global_hidden
  .call c
.LBBentry563:
  mov.i64   $0, 10

  mov.i64   $1, global
  load.8.i64  $2, [$1]
  load.8.i64  $3, [$2]
  load.8.i64  $4, [$3]
  load.8.i64  $5, [$4]
  mov.i64   $6, 8
  add.i64   $7, $5, $6
  store.8      [$7], $0

  ret
  .end

  .section .data
global:
  .long 0

