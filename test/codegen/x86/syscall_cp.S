  .section .text
__syscall_cp_asm:
  .args     i64, i64, i64, i64, i64, i64, i64, i64
__cp_begin:
  arg.i64   $0, 0 # cancel
  arg.i64   $1, 1 # nr
  arg.i64   $2, 2 # arg0
  arg.i64   $3, 3 # arg1
  arg.i64   $4, 4 # arg2
  arg.i64   $5, 5 # arg3
  arg.i64   $6, 6 # arg4
  arg.i64   $7, 7 # arg5

  ld.i32    $8, [$0]
  mov.i32   $9, 0
  cmp.ne.i8 $10, $8, $9
  jcc       $10, __cp_cancel, Lcont
Lcont:
  syscall.i64  $11, $1, $2, $3, $4, $5, $6, $7
  jmp __cp_end
__cp_end:
  ret     $11
__cp_cancel:
  mov.i64     $12, __cancel
  tcall.i32.c $12
  .end
