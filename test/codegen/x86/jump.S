  .section .text
_jmp:
  .call        c
  jmp          .BB
.BB:
  ret
  .end

  .section .text
_jt:
  .args        i32
  .call        c
  arg.i32      $0, 0
  jt           $0, .Btrue_0
  mov.i64      $1, 0
  ret.i64      $1
.Btrue_0:
  mov.i64      $1, 1
  ret.i64      $1
  .end


  .section .text
_jf:
  .args        i32
  .call        c
  arg.i32      $0, 0
  jf           $0, .Btrue_1
  mov.i64      $1, 0
  ret.i64      $1
.Btrue_1:
  mov.i64      $1, 1
  ret.i64      $1
  .end

  .section .text
_ji:
  .args        i64, i64
  .call        c
  arg.i64      $0, 0
  arg.i64      $1, 1
  raise        $0, $1
  .end
