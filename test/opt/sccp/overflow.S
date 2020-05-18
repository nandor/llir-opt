# ARGS: -passes=sccp,dead-code-elim -emit=llir


# CHECK: uadd_1
uadd_1:
  mov.i8   $0, 255
  mov.i8   $1, 255
  uaddo.i8 $2, $0, $1
  # CHECK: mov.i8 $0, 1
  # CHECK: ret $0
  ret      $2

# CHECK: uadd_0
uadd_0:
  mov.i8   $0, 0
  mov.i8   $1, 0
  uaddo.i8 $2, $0, $1
  # CHECK: mov.i8 $0, 0
  # CHECK: ret $0
  ret      $2


# CHECK: sadd_1
sadd_1:
  mov.i8   $0, 127
  mov.i8   $1, 127
  saddo.i8 $2, $0, $1
  # CHECK: mov.i8 $0, 1
  # CHECK: ret $0
  ret      $2

# CHECK: sadd_0
sadd_0:
  mov.i8   $0, 0
  mov.i8   $1, 0
  saddo.i8 $2, $0, $1
  # CHECK: mov.i8 $0, 0
  # CHECK: ret $0
  ret      $2