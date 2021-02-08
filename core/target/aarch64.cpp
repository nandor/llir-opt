// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/target/aarch64.h"



// -----------------------------------------------------------------------------
AArch64Target::AArch64Target(
    const llvm::Triple &triple,
    const std::string &cpu,
    const std::string &tuneCPU,
    const std::string &fs,
    const std::string &abi,
    bool shared)
  : Target(kKind, triple, cpu, tuneCPU, fs, abi, shared)
{
}
