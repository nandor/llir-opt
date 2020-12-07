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
class ReturnInst;
class RaiseInst;
class LandingPadInst;



/**
 * Calling convention analysis.
 */
class CallLowering {
public:
  /// Location storing the part of an argument.
  struct ArgPart {
    /// Location: register or stack.
    enum Kind {
      REG,
      STK,
    };

    /// Location kind.
    Kind K;
    /// Target value type.
    llvm::MVT VT;

    /// Register assigned to.
    llvm::Register Reg = 0u;

    /// Stack index.
    unsigned Offset = 0;
    /// Size on stack.
    unsigned Size = 0;

    ArgPart(
        llvm::MVT vt,
        llvm::Register reg)
      : K(Kind::REG)
      , VT(vt)
      , Reg(reg)
    {
    }

    ArgPart(
        llvm::MVT vt,
        unsigned offset,
        unsigned size)
      : K(Kind::STK)
      , VT(vt)
      , Offset(offset)
      , Size(size)
    {
    }
  };

  /// Location of an argument.
  struct ArgLoc {
    /// Argument index.
    unsigned Index;
    /// Type of the argument.
    Type ArgType;
    /// Parts of the argument.
    llvm::SmallVector<ArgPart, 2> Parts;

    ArgLoc(unsigned index, Type argType) : Index(index), ArgType(argType) {}
  };

  // Iterator over the arguments.
  using arg_iterator = std::vector<ArgLoc>::iterator;
  using const_arg_iterator = std::vector<ArgLoc>::const_iterator;

  /// Storage for a return value.
  struct RetPart {
    /// Original value type.
    llvm::MVT VT;
    /// Register assigned to.
    llvm::Register Reg;

    RetPart(llvm::MVT partVT, llvm::Register reg)
      : VT(partVT), Reg(reg)
    {
    }
  };

  /// Location of a return value.
  struct RetLoc {
    /// Index of the return value.
    unsigned Index;
    /// Parts of the return value.
    llvm::SmallVector<RetPart, 2> Parts;

    RetLoc(unsigned index) : Index(index) {}
  };

  // Iterator over the returns.
  using ret_iterator = std::vector<RetLoc>::iterator;
  using const_ret_iterator = std::vector<RetLoc>::const_iterator;

public:
  CallLowering(const Func *func);
  CallLowering(const CallSite *call);
  CallLowering(const RaiseInst *inst);
  CallLowering(const LandingPadInst *inst);
  CallLowering(const ReturnInst *inst);

  virtual ~CallLowering();

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return args_.size(); }
  /// Returns the size of the call frame.
  virtual unsigned GetFrameSize() const = 0;

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

  // Iterator over argument info.
  ret_iterator ret_begin() { return rets_.begin(); }
  ret_iterator ret_end() { return rets_.end(); }
  const_ret_iterator ret_begin() const { return rets_.begin(); }
  const_ret_iterator ret_end() const { return rets_.end(); }

  /// Returns a range over all returns.
  llvm::iterator_range<ret_iterator> rets()
  {
    return llvm::make_range(ret_begin(), ret_end());
  }

  /// Return an immutable range over all returns.
  llvm::iterator_range<const_ret_iterator> rets() const
  {
    return llvm::make_range(ret_begin(), ret_end());
  }

  /// Returns the type of a return value.
  const RetLoc &Return(unsigned idx) const { return rets_[idx]; }

protected:
  /// Location assignment for C.
  virtual void AssignArgC(unsigned i, Type type) = 0;
  /// Location assignment for Ocaml.
  virtual void AssignArgOCaml(unsigned i, Type type) = 0;
  /// Location assignment for OCaml to C allocator calls.
  virtual void AssignArgOCamlAlloc(unsigned i, Type type) = 0;
  /// Location assignment for OCaml to GC trampolines.
  virtual void AssignArgOCamlGc(unsigned i, Type type) = 0;
  /// Location assignment for Xen hypercalls.
  virtual void AssignArgXen(unsigned i, Type type) = 0;

  /// Location assignment for C.
  virtual void AssignRetC(unsigned i, Type type) = 0;
  /// Location assignment for Ocaml.
  virtual void AssignRetOCaml(unsigned i, Type type) = 0;
  /// Location assignment for OCaml to C allocator calls.
  virtual void AssignRetOCamlAlloc(unsigned i, Type type) = 0;
  /// Location assignment for OCaml to GC trampolines.
  virtual void AssignRetOCamlGc(unsigned i, Type type) = 0;
  /// Location assignment for Xen hypercalls.
  virtual void AssignRetXen(unsigned i, Type type) = 0;

protected:
  /// Analyse a function.
  void AnalyseFunc(const Func *func);
  /// Analyse a call.
  void AnalyseCall(const CallSite *call);
  /// Analyse a return instruction.
  void AnalyseReturn(const ReturnInst *inst);
  /// Analyse a raise instruction.
  void AnalyseRaise(const RaiseInst *inst);
  /// Analyse a landing pad instruction.
  void AnalysePad(const LandingPadInst *inst);

  /// Assigns a location to an argument based on calling conv.
  void AssignArg(unsigned i, Type type);
  /// Assigns a location to a return value based on callig conv.
  void AssignRet(unsigned i, Type type);

protected:
  /// Calling convention.
  CallingConv conv_;
  /// Locations where arguments are assigned.
  std::vector<ArgLoc> args_;
  /// Locations where return values are assigned.
  std::vector<RetLoc> rets_;
};
