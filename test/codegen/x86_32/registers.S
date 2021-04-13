# RUN: %opt - -triple i386 -verify

  .section .text
test:
  #get.i32     $1100, $x86_cr0
  #mov.i32     $1101, 0x9fffffff
  #and.i32     $1102, $1100, $1101
  #set.i32     $x86_cr0, $1102
  #mov.i32     $1200, _gdt_rom
  #x86_lgdt    $1200
  #mov.i32     $1201, 0x8
  #set         $x86_ds, $1201
  #set         $x86_es, $1201
  #set         $x86_ss, $1201
  #mov.i32     $1202, 0
  #set         $x86_fs, $1202
  #set         $x86_gs, $1202
  #mov.i32     $1203, 8
  #set         $x86_cs, $1203
  #get.i32     $1300, $x86_cr0
  #mov.i32     $1301, 0x2e
  #or.i32      $1302, $1300, $1301
  #set.i32     $x86_cr0, $1302
  ret
  .end

  .section .data
_gdt_rom:
  .quad 0
  .end
