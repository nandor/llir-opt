  .section .text
caml_request_major_slice:
  .visibility hidden
  .call c
.LBBentry563:
  mov.i64   $0, 10

  mov.i64   $1, global
  ld.8.i64  $2, [$1]
  ld.8.i64  $3, [$2]
  ld.8.i64  $4, [$3]
  ld.8.i64  $5, [$4]
  mov.i64   $6, 8
  add.i64   $7, $5, $6
  st.8      [$7], $0

  ret
  .end

  .section .data
global:
  .long 0

