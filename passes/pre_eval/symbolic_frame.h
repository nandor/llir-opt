// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>

#include "core/func.h"
#include "passes/pre_eval/symbolic_object.h"



/// Information about a frame.
class SymbolicFrame {
public:
  /// Create a new frame.
  SymbolicFrame(Func &func, unsigned index, llvm::ArrayRef<SymbolicValue> args);
  /// Create a new top-level frame.
  SymbolicFrame(unsigned index, llvm::ArrayRef<Func::StackObject> objects);
  /// Copy the frame.
  SymbolicFrame(const SymbolicFrame &that);

  /// Return the function.
  const Func *GetFunc() const { return func_; }
  /// Return the index.
  unsigned GetIndex() const { return index_; }
  /// De-activate the frame.
  void Leave() { valid_ = false; }
  /// Return a specific object.
  SymbolicFrameObject &GetObject(unsigned object) { return *objects_[object]; }
  /// Check if the frame is valid.
  bool IsValid() const { return valid_; }

  /**
   * Map an instruction producing a single value to a new value.
   *
   * @return True if the value changed.
   */
  bool Set(Ref<Inst> i, const SymbolicValue &value);

  /// Return the value an instruction was mapped to.
  const SymbolicValue &Find(ConstRef<Inst> inst);
  /// Return the value, if it was already defined.
  const SymbolicValue *FindOpt(ConstRef<Inst> inst);
  /// Return the value of an argument.
  const SymbolicValue &Arg(unsigned index) { return args_[index]; }

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return args_.size(); }

  /**
   * Merges another frame into this one.
   */
  void LUB(const SymbolicFrame &that);

private:
  /// Initialise all objects.
  void Initialise(llvm::ArrayRef<Func::StackObject> objects);

private:
  /// Reference to the function.
  const Func *func_;
  /// Unique index for the frame.
  unsigned index_;
  /// Flag to indicate whether the index is valid.
  bool valid_;
  /// Arguments to the function.
  std::vector<SymbolicValue> args_;
  /// Mapping from object IDs to objects.
  std::map<unsigned, std::unique_ptr<SymbolicFrameObject>> objects_;
  /// Mapping from instructions to their symbolic values.
  std::unordered_map<ConstRef<Inst>, SymbolicValue> values_;
};
