// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/register.h"


// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Register reg)
{
  switch (reg) {
    case Register::SP:           return os << "$sp";
    case Register::FS:           return os << "$fs";
    case Register::RET_ADDR:     return os << "$ret_addr";
    case Register::FRAME_ADDR:   return os << "$frame_addr";
    case Register::AARCH64_FPSR: return os << "$aarch64_fpsr";
    case Register::AARCH64_FPCR: return os << "$aarch64_fpcr";
    case Register::RISCV_FFLAGS: return os << "$riscv_fflags";
    case Register::RISCV_FRM:    return os << "$riscv_frm";
    case Register::RISCV_FCSR:   return os << "$riscv_fcsr";
    case Register::PPC_FPSCR:    return os << "$ppc_fpscr";
  }
  llvm_unreachable("invalid register");
}
