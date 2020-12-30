// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>

#include "passes/pre_eval/symbolic_object.h"
#include "passes/pre_eval/symbolic_frame.h"

class Atom;
class Prog;
class Object;
class SymbolicFrame;



/**
 * Symbolic representation of the heap.
 */
class SymbolicContext final {
public:
  /// Creates a new heap using values specified in the data segments.
  SymbolicContext(Prog &prog);
  /// Cleanup.
  ~SymbolicContext();

  /// Set a value in the topmost frame.
  bool Set(Inst &i, const SymbolicValue &value)
  {
    return frames_.rbegin()->Set(i, value);
  }

  /// Find a value in the topmost frame.
  const SymbolicValue &Find(ConstRef<Inst> inst)
  {
    return frames_.rbegin()->Find(inst);
  }

  /// Find a value in the topmost frame.
  const SymbolicValue *FindOpt(ConstRef<Inst> inst)
  {
    return frames_.rbegin()->FindOpt(inst);
  }

  /// Return the value of an argument in the topmost frame.
  const SymbolicValue &Arg(unsigned index)
  {
    return frames_.rbegin()->Arg(index);
  }

  /// Push a stack frame for a function to the heap.
  unsigned EnterFrame(Func &func, llvm::ArrayRef<SymbolicValue> args);
  /// Push the initial stack frame.
  unsigned EnterFrame(llvm::ArrayRef<Func::StackObject> objects);
  /// Pop a stack frame for a function from the heap.
  void LeaveFrame(Func &func);
  /// Returns the ID of this frame.
  unsigned CurrentFrame() { return frames_.size() - 1; }

  /// Returns an object to store to.
  SymbolicDataObject &GetObject(Atom *atom);

  /// Returns a frame object to store to.
  SymbolicFrameObject &GetFrame(unsigned frame, unsigned object)
  {
    return frames_[frame].GetObject(object);
  }

  /**
   * Stores a value to the symbolic heap representation.
   *
   * If the value is stored to a precise location, the heap is updated to
   * reflect the result of the store. Otherwise, the whole range of addresses
   * is invalidated in order to over-approximate unknown stores.
   */
  bool Store(
      const SymbolicPointer &addr,
      const SymbolicValue &val,
      Type type
  );

  /**
   * Loads a value from the symbolic heap representation.
   */
  SymbolicValue Load(const SymbolicPointer &addr, Type type);

private:
  /// Performs a store to a precise pointer.
  bool StoreGlobal(
      Global *g,
      int64_t offset,
      const SymbolicValue &val,
      Type type
  );
  /// Performs load from a precise pointer.
  SymbolicValue LoadGlobal(Global *g, int64_t offset, Type type);

  /// Taints a global due to an imprecise
  bool StoreGlobalImprecise(
      Global *g,
      int64_t offset,
      const SymbolicValue &val,
      Type type
  );

  /// Taints a global due to an imprecise pointer.
  bool StoreGlobalImprecise(
      Global *g,
      const SymbolicValue &val,
      Type type
  );

  /// Performs a store to an external pointer.
  bool StoreExtern(const SymbolicValue &val);
  /// Performs a load from an external pointer.
  SymbolicValue LoadExtern();


private:
  /// Mapping from heap-allocated objects to their symbolic values.
  std::unordered_map<
      Object *,
      std::unique_ptr<SymbolicDataObject>
  > objects_;

  /// Stack of frames.
  std::vector<SymbolicFrame> frames_;
};
