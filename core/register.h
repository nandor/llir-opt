// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>



/// Enumeration of hardware registers.
enum class Register : uint8_t {
  /// Stack pointer.
  SP,
  /// Thread descriptor.
  FS,
  /// Virtual register taking the value of the return address.
  RET_ADDR,
  /// Virtual register taking the value of the top of the stack.
  FRAME_ADDR,
  /// X86_64 CR2 register.
  X86_CR2,
  /// X86_64 CR3 register.
  X86_CR3,
  /// AArch64 FPSR register.
  AARCH64_FPSR,
  /// AArch64 FPCR register.
  AARCH64_FPCR,
  /// RISC-V fflags register.
  RISCV_FFLAGS,
  /// RISC-V frm register.
  RISCV_FRM,
  /// RISC-V fcsr register.
  RISCV_FCSR,
  /// PowerPC fp status register.
  PPC_FPSCR,
};

/**
 * Prints the register to a stream.
 */
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Register reg);
