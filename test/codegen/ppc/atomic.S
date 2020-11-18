# RUN: %opt - -triple powerpc64le


test_sync:
  # CHECK: sync
  ppc_sync
  ret
  .end


test_isync:
  # CHECK: isync
  ppc_isync
  ret
  .end
