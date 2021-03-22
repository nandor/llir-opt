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
  /// X86 CR0 register.
  X86_CR0,
  /// X86 CR2 register.
  X86_CR2,
  /// X86 CR3 register.
  X86_CR3,
  /// X86 CR4 register.
  X86_CR4,
  /// X86 Data Segment.
  X86_DS,
  /// X86 Extra Segment.
  X86_ES,
  /// X86 Stack Segment.
  X86_SS,
  /// X86 General Purpose Segment.
  X86_FS,
  /// X86 General Purpose Segment.
  X86_GS,
  /// X86 Code Segment.
  X86_CS,
  /// AArch64 FPSR register.
  AARCH64_FPSR,
  /// AArch64 FPCR register.
  AARCH64_FPCR,
  /// AArch64 Counter-Timer Virtual Count Register.
  AARCH64_CNTVCT,
  /// AArch64 Counter-Timer Virtual Count Frequency.
  AARCH64_CNTFRQ,
  /// AArch64 Fault Address Register.
  AARCH64_FAR,
  /// AArch64 Vector Base Address Register.
  AARCH64_VBAR,
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
