# RUN: %opt - -triple x86_64


test_lgdt:
  .args   i64

  arg.i64   $0, 0
  x86_lgdt  $0
  ret
  .end

test_set_cs:
  .args   i64

  arg.i64    $0, 0
  x86_set_cs $0
  ret
  .end

test_set_ds:
  .args   i64

  arg.i64     $0, 0
  x86_set_ds  $0
  ret
  .end
