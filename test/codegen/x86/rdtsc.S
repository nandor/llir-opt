# ARGS: -O1

ocaml_read_time_stamp_counter:
  .visibility          extern
  .call                c
  mov.i64              $1, 1
  # CHECK: rdtsc
  # CHECK: orq
  rdtsc.i64            $2
  sll.i64              $3, $2, $1
  or.i64               $4, $3, $1
  ret                  $4
