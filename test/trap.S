  .text
_trap:
  .args        1, 0
  .call        c
  # ud2
  trap
