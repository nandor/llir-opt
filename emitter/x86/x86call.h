// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

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
    /// Argument value.
    const Inst *Value;
  };

  // Iterator over the arguments.
  using arg_iterator = std::vector<Loc>::iterator;
  using const_arg_iterator = std::vector<Loc>::const_iterator;

public:
  /// Analyses a call site.
  X86Call(const CallInst *call);
  /// Analyses an entire function.
  X86Call(const Func *func);

  // Iterator over argument info.
  arg_iterator arg_begin() { return args_.begin(); }
  arg_iterator arg_end() { return args_.end(); }
  const_arg_iterator arg_begin() const { return args_.begin(); }
  const_arg_iterator arg_end() const { return args_.end(); }

  /// Returns a given argument.
  const Loc &operator [] (size_t idx) const { return args_[idx]; }

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return args_.size(); }

  /// Returns the size of the call frame.
  unsigned GetFrameSize() const { return stack_; }

private:
  /// Assigns an argument of a specific type to a location.
  void Assign(unsigned i, Type type, const Inst *value);

private:
  /// Locations where arguments are assigned to.
  std::vector<Loc> args_;
  /// Last stack index.
  uint64_t stack_;
};
