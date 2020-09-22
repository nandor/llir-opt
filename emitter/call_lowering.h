// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once



/**
 * Calling convention analysis.
 */
class CallLowering {
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
    Type ArgType;
    /// Value passed to a call.
    const Inst *Value;
  };

  // Iterator over the arguments.
  using arg_iterator = std::vector<Loc>::iterator;
  using const_arg_iterator = std::vector<Loc>::const_iterator;

public:
  /// Returns the location where an argument is available on entry.
  virtual const Loc &operator [] (size_t idx) const = 0;
};
