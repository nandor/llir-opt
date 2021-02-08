// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/target.h"



/**
 * AArch64 target information.
 */
class AArch64Target final : public Target {
private:
  /// Kind for RTTI.
  static const Kind kKind = Kind::AARCH64;

public:
  AArch64Target(
      const llvm::Triple &triple,
      const std::string &cpu,
      const std::string &tuneCPU,
      const std::string &fs,
      const std::string &abi,
      bool shared)
    : Target(kKind, triple, cpu, tuneCPU, fs, abi, shared)
  {
  }

private:
  friend class Target;
};
