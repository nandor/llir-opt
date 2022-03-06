# RUN: %opt - -triple x86_64

test:
  # CHECK:  .cfi_personality 155, DW.ref.__gxx_personality_v0
  .personality __gxx_personality_v0
  mov.i64           $0, raising
  invoke.c          $0, .Lcont, .Lpad
.Lcont:
  trap
.Lpad:
  landing_pad.i64   $1
  ret               $1
  .end

  # CHECK: DW.ref.__gxx_personality_v0:
