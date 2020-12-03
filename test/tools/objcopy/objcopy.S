# RUN: %objcopy - - -emit=llir -w -G global*


  .section .text
global_1:
  # CHECK: global_1
  # CHECK: .visibility global_default
  .visibility global_default
  trap
  .end

  .section .text
global_2:
  # CHECK: global_2
  # CHECK: .visibility global_default
  .visibility global_default
  trap
  .end

  .section .text
local_1:
  # CHECK: local_1
  # CHECK: .visibility local
  .visibility global_default
  trap
  .end

  .section .text
local_2:
  # CHECK: local_2
  # CHECK: .visibility local
  .visibility global_default
  trap
  .end
