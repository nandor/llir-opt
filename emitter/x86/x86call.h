// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator_range.h>

#include "core/inst.h"

class CallInst;
class InvokeInst;
class TailCallInst;
class ReturnInst;
class Func;



/**
 * X86 calling convention analysis.
 */
class X86Call final {
public:
  /// Structure holding information about the location of an argument.
  struct Loc {
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
    unsigned Reg;
    /// Stack index.
    unsigned Idx;
    /// Size on stack.
    unsigned Size;
    /// Type of the argument.
    Type Type;
    /// Value passed to a call.
    const Inst *Value;
  };

  // Iterator over the arguments.
  using arg_iterator = std::vector<Loc>::iterator;
  using const_arg_iterator = std::vector<Loc>::const_iterator;

public:
  /// Analyses an entire function.
  X86Call(const Func *func);

  /// Analyses a call site.
  template<typename T>
  X86Call(const CallSite<T> *call, bool isVarArg, bool isTailCall)
    : conv_(call->GetCallingConv())
    , args_(call->GetNumArgs())
    , stack_(0ull)
    , regs_(0)
    , xmms_(0)
  {
    // Handle fixed args.
    auto it = call->arg_begin();
    for (unsigned i = 0, nargs = call->GetNumArgs(); i < nargs; ++i, ++it) {
      Assign(i, (*it)->GetType(0), *it);
    }
  }

  // Iterator over argument info.
  arg_iterator arg_begin() { return args_.begin(); }
  arg_iterator arg_end() { return args_.end(); }
  const_arg_iterator arg_begin() const { return args_.begin(); }
  const_arg_iterator arg_end() const { return args_.end(); }

  llvm::iterator_range<arg_iterator> args()
  {
    return llvm::make_range(arg_begin(), arg_end());
  }

  llvm::iterator_range<const_arg_iterator> args() const
  {
    return llvm::make_range(arg_begin(), arg_end());
  }


  /// Returns a given argument.
  const Loc &operator [] (size_t idx) const { return args_[idx]; }

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return args_.size(); }
  /// Returns the size of the call frame.
  unsigned GetFrameSize() const { return stack_; }

  /// Returns unused GPRs.
  llvm::ArrayRef<unsigned> GetUnusedGPRs() const;
  /// Returns the used GPRs.
  llvm::ArrayRef<unsigned> GetUsedGPRs() const;
  /// Returns unused XMMs.
  llvm::ArrayRef<unsigned> GetUnusedXMMs() const;
  /// Returns the used XMMs.
  llvm::ArrayRef<unsigned> GetUsedXMMs() const;

private:
  /// Assigns a location to an argument based on calling conv.
  void Assign(unsigned i, Type type, const Inst *value);
  /// Location assignment for C and Fast on x86-64.
  void AssignC(unsigned i, Type type, const Inst *value);
  /// Location assignment for Ocaml on X86-64.
  void AssignOCaml(unsigned i, Type type, const Inst *value);
  /// Location assignment for OCaml to C calls.
  void AssignOCamlExt(unsigned i, Type type, const Inst *value);
  /// Location assignment for OCaml to C allocator calls.
  void AssignOCamlAlloc(unsigned i, Type type, const Inst *value);
  /// Location assignment for OCaml to GC trampolines.
  void AssignOCamlGc(unsigned i, Type type, const Inst *value);

  /// Assigns a location to a register.
  void AssignReg(unsigned i, Type type, const Inst *value, unsigned reg);
  /// Assigns a location to an XMM register.
  void AssignXMM(unsigned i, Type type, const Inst *value, unsigned reg);
  /// Assigns a location to the stack.
  void AssignStack(unsigned i, Type type, const Inst *value);

  /// Returns the list of GPR registers.
  llvm::ArrayRef<unsigned> GetGPRs() const;
  /// Returns the list of XMM registers.
  llvm::ArrayRef<unsigned> GetXMMs() const;

private:
  /// Calling convention.
  CallingConv conv_;
  /// Locations where arguments are assigned to.
  std::vector<Loc> args_;
  /// Last stack index.
  uint64_t stack_;
  /// Number of arguments in regular registers.
  uint64_t regs_;
  /// Number of arguments in vector registers.
  uint64_t xmms_;
};
