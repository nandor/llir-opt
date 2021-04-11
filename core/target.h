// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/Triple.h>

#include "core/type.h"



/// Forward declaration of target kinds.
class X86Target;
class PPCTarget;
class AArch64Target;
class RISCVTarget;



/**
 * Helper class wrapping information about specific targets.
 */
class Target {
public:
  /// Enumeration of supported targets.
  enum class Kind {
    X86,
    PPC,
    AARCH64,
    RISCV
  };

public:
  Target(
      Kind kind,
      const llvm::Triple &triple,
      const std::string &cpu,
      const std::string &tuneCPU,
      const std::string &fs,
      const std::string &abi,
      bool shared)
    : kind_(kind)
    , triple_(triple)
    , cpu_(cpu)
    , tuneCPU_(tuneCPU)
    , fs_(fs)
    , abi_(abi)
    , shared_(shared)
  {
  }

  /// Convert the target to a specific target.
  template<typename T>
  T *As()
  {
    return T::kKind == kind_ ? static_cast<T *>(this) : nullptr;
  }

  /// Convert the target to a specific target.
  template<typename T>
  const T *As() const
  {
    return T::kKind == kind_ ? static_cast<const T *>(this) : nullptr;
  }

  /// Checks whether the target is a shared library.
  bool IsShared() const { return shared_; }
  /// Returns the target triple.
  const llvm::Triple &GetTriple() const { return triple_; }
  /// Returns the CPU to target.
  llvm::StringRef getCPU() const { return cpu_; }
  /// Returns the CPU to target.
  llvm::StringRef getTuneCPU() const { return tuneCPU_; }
  /// Return the feature strings of the target.
  llvm::StringRef getFS() const { return fs_; }
  /// Return the feature strings of the target.
  llvm::StringRef getABI() const { return abi_; }

  /// Return the target pointer type.
  Type GetPointerType() const;

  /// Check whether the target is little endian.
  virtual bool IsLittleEndian() const { return true; }
  /// Check whether the target allows unaligned stores.
  virtual bool AllowsUnalignedStores() const { return false; }
 
protected:
  /// Target kind.
  Kind kind_;
  /// Target triple.
  llvm::Triple triple_;
  /// Target CPU.
  std::string cpu_;
  /// Target CPU to tune fore.
  std::string tuneCPU_;
  /// Target feature string.
  std::string fs_;
  /// Target ABI descriptor.
  std::string abi_;
  /// Flag indicating whether the target is a shared library.
  bool shared_;
};
