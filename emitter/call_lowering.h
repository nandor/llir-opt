// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/CodeGen/Register.h>
#include <llvm/CodeGen/TargetRegisterInfo.h>

#include "core/type.h"
#include "core/calling_conv.h"

class Inst;
class Func;
class CallSite;



/**
 * Calling convention analysis.
 */
class CallLowering {
public:
  /// Location of an argument.
  struct ArgLoc {
    /// Location: register or stack.
    enum Kind {
      REG,
      STK,
    };

    /// Argument index.
    unsigned Index;
    /// Location kind.
    Kind Kind;
    /// Register assigned to.
    llvm::Register Reg;
    /// The register class.
    const llvm::TargetRegisterClass *RegClass;
    /// Target value type.
    llvm::MVT VT;
    /// Stack index.
    unsigned Idx;
    /// Size on stack.
    unsigned Size;
    /// Type of the argument.
    Type ArgType;
    /// Value passed to a call.
    const Inst *Value;
  };

  /// Location of a return value.
  struct RetLoc {
    /// Register assigned to.
    llvm::Register Reg;
    /// MVT of the argument.
    llvm::MVT VT;
  };

  // Iterator over the arguments.
  using arg_iterator = std::vector<ArgLoc>::iterator;
  using const_arg_iterator = std::vector<ArgLoc>::const_iterator;

public:
  CallLowering(const Func *func);
  CallLowering(const CallSite *call);

  virtual ~CallLowering();

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return args_.size(); }
  /// Returns the size of the call frame.
  unsigned GetFrameSize() const { return stack_; }

  // Iterator over argument info.
  arg_iterator arg_begin() { return args_.begin(); }
  arg_iterator arg_end() { return args_.end(); }
  const_arg_iterator arg_begin() const { return args_.begin(); }
  const_arg_iterator arg_end() const { return args_.end(); }

  /// Returns a range over all arguments.
  llvm::iterator_range<arg_iterator> args()
  {
    return llvm::make_range(arg_begin(), arg_end());
  }

  /// Return an immutable range over all arguments.
  llvm::iterator_range<const_arg_iterator> args() const
  {
    return llvm::make_range(arg_begin(), arg_end());
  }

  /// Returns a given argument.
  const ArgLoc &Argument(size_t idx) const { return args_[idx]; }
  /// Returns the type of a return value.
  virtual RetLoc Return(Type type) const = 0;

protected:
  /// Location assignment for C.
  virtual void AssignC(unsigned i, Type type, const Inst *value) = 0;
  /// Location assignment for Ocaml.
  virtual void AssignOCaml(unsigned i, Type type, const Inst *value) = 0;
  /// Location assignment for OCaml to C allocator calls.
  virtual void AssignOCamlAlloc(unsigned i, Type type, const Inst *value) = 0;
  /// Location assignment for OCaml to GC trampolines.
  virtual void AssignOCamlGc(unsigned i, Type type, const Inst *value) = 0;

protected:
  /// Analyse a call.
  void AnalyseCall(const CallSite *call);
  /// Analyse a function.
  void AnalyseFunc(const Func *func);
  /// Assigns a location to an argument based on calling conv.
  void Assign(unsigned i, Type type, const Inst *value);

protected:
  /// Calling convention.
  CallingConv conv_;
  /// Locations where arguments are assigned to.
  std::vector<ArgLoc> args_;
  /// Last stack index.
  uint64_t stack_;
};
