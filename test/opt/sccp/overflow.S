# RUN: %opt - -pass=sccp -pass=dead-code-elim -emit=llir


# CHECK: uadd_1
uadd_1:
  .visibility global_default
  mov.i8   $0, 255
  mov.i8   $1, 255
  uaddo.i8 $2, $0, $1
  # CHECK: mov i8:$0, 1
  # CHECK: return $0
  ret      $2
  .end

# CHECK: uadd_0
uadd_0:
  .visibility global_default
  mov.i8   $0, 0
  mov.i8   $1, 0
  uaddo.i8 $2, $0, $1
  # CHECK: mov i8:$0, 0
  # CHECK: return $0
  ret      $2
  .end


# CHECK: sadd_1
sadd_1:
  .visibility global_default
  mov.i8   $0, 127
  mov.i8   $1, 127
  saddo.i8 $2, $0, $1
  # CHECK: mov i8:$0, 1
  # CHECK: return $0
  ret      $2
  .end

# CHECK: sadd_0
sadd_0:
  .visibility global_default
  mov.i8   $0, 0
  mov.i8   $1, 0
  saddo.i8 $2, $0, $1
  # CHECK: mov i8:$0, 0
  # CHECK: return $0
  ret      $2
  .end
