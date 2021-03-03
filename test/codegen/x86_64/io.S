# RUN: %opt - -triple x86_64

# CHECK: inb
test_in_8:
  mov.i16    $1, 100
  x86_in.i8  $2, $1
  ret        $2
  .end

# CHECK: inw
test_in_16:
  mov.i16    $1, 100
  x86_in.i16 $2, $1
  ret        $2
  .end

# CHECK: inl
test_in_32:
  mov.i16    $1, 100
  x86_in.i32 $2, $1
  ret        $2
  .end

# CHECK: outb
test_out_8:
  mov.i16    $1, 100
  mov.i8     $2, 2
  x86_out    $1, $2
  ret
  .end

# CHECK: outw
test_out_16:
  mov.i16    $1, 100
  mov.i16     $2, 2
  x86_out    $1, $2
  ret
  .end

# CHECK: outl
test_out_32:
  mov.i16    $1, 100
  mov.i32     $2, 2
  x86_out    $1, $2
  ret
  .end

