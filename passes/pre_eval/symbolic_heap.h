// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>

#include "passes/pre_eval/symbolic_object.h"

class Atom;
class Prog;
class Object;
class SymbolicFrame;



/// Information about a frame.
class SymbolicFrame {
public:
  /// Create a new frame.
  SymbolicFrame(Func &func, unsigned index);

  /// Return the function.
  const Func &GetFunc() const { return func_; }
  /// De-activate the frame.
  void Leave() { valid_ = false; }
  /// Return a specific object.
  SymbolicFrameObject &GetObject(unsigned object) { return *objects_[object]; }

private:
  /// Reference to the function.
  const Func &func_;
  /// Unique index for the frame.
  unsigned index_;
  /// Flag to indicate whether the index is valid.
  bool valid_;
  /// Mapping from object IDs to objects.
  std::map<unsigned, std::unique_ptr<SymbolicFrameObject>> objects_;
};

/**
 * Symbolic representation of the heap.
 */
class SymbolicHeap final {
public:
  /// Creates a new heap using values specified in the data segments.
  SymbolicHeap(Prog &prog);
  /// Cleanup.
  ~SymbolicHeap();

  /// Push a stack frame for a function to the heap.
  void EnterFrame(Func &func);
  /// Pop a stack frame for a function from the heap.
  void LeaveFrame(Func &func);
  /// Returns the ID of this frame.
  unsigned CurrentFrame() { return frames_.size() - 1; }

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

  /// Returns an object to store to.
  SymbolicDataObject &GetObject(Atom *atom);
  /// Returns a frame object to store to.
  SymbolicFrameObject &GetFrame(unsigned frame, unsigned object)
  {
    return frames_[frame].GetObject(object);
  }

private:
  /// Mapping from heap-allocated objects to their symbolic values.
  std::unordered_map<
      Object *,
      std::unique_ptr<SymbolicDataObject>
  > objects_;

  /// Stack of frames.
  std::vector<SymbolicFrame> frames_;
};
