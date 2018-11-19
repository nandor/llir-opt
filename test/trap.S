  .text
_trap:
  .args        1
  .call        c
  # ud2
  trap
