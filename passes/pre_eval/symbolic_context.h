// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>
#include <stack>

#include "passes/pre_eval/symbolic_object.h"
#include "passes/pre_eval/symbolic_frame.h"

class Atom;
class Prog;
class Object;
class SymbolicFrame;
class CallSite;



/**
 * Symbolic representation of the heap.
 */
class SymbolicContext final {
public:
  /// Creates a new heap using values specified in the data segments.
  SymbolicContext(Prog &prog);
  /// Copies an existing context.
  SymbolicContext(const SymbolicContext &that);
  /// Cleanup.
  ~SymbolicContext();

  /// Return the top frame.
  SymbolicFrame &GetActiveFrame()
  {
    return frames_[activeFrames_.top()];
  }

  /// Return the top frame.
  const SymbolicFrame &GetActiveFrame() const
  {
    return const_cast<SymbolicContext *>(this)->GetActiveFrame();
  }

  /// Set a value in the topmost frame.
  bool Set(Ref<Inst> i, const SymbolicValue &value)
  {
    return GetActiveFrame().Set(i, value);
  }

  /// Find a value in the topmost frame.
  const SymbolicValue &Find(ConstRef<Inst> inst)
  {
    return GetActiveFrame().Find(inst);
  }

  /// Find a value in the topmost frame.
  const SymbolicValue *FindOpt(ConstRef<Inst> inst)
  {
    return GetActiveFrame().FindOpt(inst);
  }

  /// Return the value of an argument in the topmost frame.
  const SymbolicValue &Arg(unsigned index)
  {
    return GetActiveFrame().Arg(index);
  }

  /// Return the number of arguments in the topmost frame.
  unsigned GetNumArgs() const { return GetActiveFrame().GetNumArgs(); }

  /// Push a stack frame for a function to the heap.
  unsigned EnterFrame(Func &func, llvm::ArrayRef<SymbolicValue> args);
  /// Push the initial stack frame.
  unsigned EnterFrame(llvm::ArrayRef<Func::StackObject> objects);
  /// Pop a stack frame for a function from the heap.
  void LeaveFrame(Func &func);
  /// Checks if a function is already on the stack.
  bool HasFrame(Func &func);

  /// Returns an object to store to.
  SymbolicDataObject &GetObject(Atom &atom);
  /// Returns an object to store to.
  SymbolicDataObject &GetObject(Object &object);

  /// Returns a frame object to store to.
  SymbolicFrameObject &GetFrame(unsigned frame, unsigned object)
  {
    return frames_[frame].GetObject(object);
  }

  /// Returns a heap object to store to.
  SymbolicHeapObject &GetHeap(CallSite &site);

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

  /**
   * Compute the closure of a set of pointers.
   */
  SymbolicPointer Taint(
      const std::set<Global *> &globals,
      const std::set<std::pair<unsigned, unsigned>> &frames,
      const std::set<CallSite *> &sites
  );

  /**
   * Compute the closure of a single pointer.
   */
  SymbolicPointer Taint(const SymbolicPointer &ptr);

  /**
   * Returns a pointer to an allocation site.
   */
  SymbolicPointer Malloc(CallSite &site, std::optional<size_t> size);

  /**
   * Merge a prior context into this one.
   */
  void LUB(SymbolicContext &that);

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
  /// Performs load from an imprecise pointer.
  SymbolicValue LoadGlobalImprecise(Global *g, Type type);

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
  /// Stack of active frame IDs.
  std::stack<unsigned> activeFrames_;

  /// Representation of allocation sites.
  std::unordered_map<
      CallSite *,
      std::unique_ptr<SymbolicHeapObject>
  > allocs_;
};
