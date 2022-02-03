# RUN: %opt - -triple x86_64 -O1

ocaml_read_time_stamp_counter:
  .visibility          global_default
  .call                c
  mov.i64              $1, 1
  # CHECK: rdtsc
  # CHECK: orq
  x86_rd_tsc.i64        $2
  sll.i64              $3, $2, $1
  or.i64               $4, $3, $1
  ret                  $4
