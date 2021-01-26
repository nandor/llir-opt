// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>
#include <stack>

#include "core/adt/id.h"
#include "passes/pre_eval/symbolic_object.h"
#include "passes/pre_eval/symbolic_frame.h"

class Atom;
class Prog;
class Object;
class SymbolicFrame;
class SymbolicSummary;
class CallSite;



/**
 * Symbolic representation of the heap.
 */
class SymbolicContext final {
public:
  /// Iterator over active frames.
  class frame_iterator : public std::iterator
    < std::forward_iterator_tag
    , SymbolicFrame *
    >
  {
  public:
    frame_iterator(
        std::vector<unsigned>::const_reverse_iterator it,
        SymbolicContext *ctx)
      : it_(it)
      , ctx_(ctx)
    {
    }

    bool operator==(const frame_iterator &that) const { return it_ == that.it_; }
    bool operator!=(const frame_iterator &that) const { return !(*this == that); }

    // Pre-increment.
    frame_iterator &operator++()
    {
      it_++;
      return *this;
    }
    // Post-increment
    frame_iterator operator++(int)
    {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    SymbolicFrame &operator*() const { return ctx_->frames_[*it_]; }
    SymbolicFrame *operator->() const { return &operator*(); }

  private:
    /// Iterator over active frame indices.
    std::vector<unsigned>::const_reverse_iterator it_;
    /// Reference to the context.
    SymbolicContext *ctx_;
  };

  /// Mapping from objects to their representation.
  using ObjectMap = std::unordered_map
      < ID<SymbolicObject>
      , std::unique_ptr<SymbolicObject>
      >;

  /// Iterator over objects.
  struct object_iterator : llvm::iterator_adaptor_base
      < object_iterator
      , ObjectMap::const_iterator
      , std::random_access_iterator_tag
      , SymbolicObject *
      >
  {
    explicit object_iterator(ObjectMap::const_iterator it)
      : iterator_adaptor_base(it)
    {
    }

    SymbolicObject &operator*() const { return *this->I->second.get(); }
    SymbolicObject *operator->() const { return &operator*(); }
  };

public:
  /// Creates a new heap using values specified in the data segments.
  SymbolicContext(SymbolicHeap &heap, SymbolicSummary &state)
    : heap_(heap)
    , state_(state)
  {
  }

  /// Copies an existing context.
  SymbolicContext(const SymbolicContext &that);
  /// Cleanup.
  ~SymbolicContext();

  /// Return the top frame.
  SymbolicFrame *GetActiveFrame();
  /// Return the top frame.
  const SymbolicFrame *GetActiveFrame() const
  {
    return const_cast<SymbolicContext *>(this)->GetActiveFrame();
  }

  /// Set a value in the topmost frame.
  bool Set(Ref<Inst> i, const SymbolicValue &value)
  {
    return GetActiveFrame()->Set(i, value);
  }

  /// Find a value in the topmost frame.
  const SymbolicValue &Find(ConstRef<Inst> inst)
  {
    return GetActiveFrame()->Find(inst);
  }

  /// Find a value in the topmost frame.
  const SymbolicValue *FindOpt(ConstRef<Inst> inst)
  {
    return GetActiveFrame()->FindOpt(inst);
  }

  /// Return the value of an argument in the topmost frame.
  const SymbolicValue &Arg(unsigned index)
  {
    return GetActiveFrame()->Arg(index);
  }

  /// Return the number of arguments in the topmost frame.
  unsigned GetNumArgs() const { return GetActiveFrame()->GetNumArgs(); }

  /// Push a stack frame for a function to the heap.
  unsigned EnterFrame(Func &func, llvm::ArrayRef<SymbolicValue> args);
  /// Push the initial stack frame.
  unsigned EnterFrame(llvm::ArrayRef<std::optional<unsigned>> objects);
  /// Pop a stack frame for a function from the heap.
  void LeaveFrame(Func &func);
  /// Pop the root frame.
  void LeaveRoot();
  /// Checks if a function is already on the stack.
  bool HasFrame(Func &func);
  /// Record a tainted value, propagating information along the call stack.
  void Taint(const SymbolicValue &taint, const SymbolicValue &tainted);

  /// Returns the model for an object.
  SymbolicObject &GetObject(ID<SymbolicObject> object);
  /// Returns the model for an object.
  SymbolicObject &GetObject(Object *object);
  /// Returns a frame object to store to.
  SymbolicObject &GetFrame(unsigned frame, unsigned object)
  {
    return GetObject(frames_[frame].GetObject(object));
  }

  /// Create a pointer to an atom.
  SymbolicPointer::Ref Pointer(Atom &atom, int64_t offset);
  /// Create a pointer to a frame object.
  SymbolicPointer::Ref Pointer(unsigned frame, unsigned object, int64_t offset);

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
   * Returns a pointer to an allocation site.
   */
  SymbolicPointer::Ref Malloc(CallSite &site, std::optional<unsigned> size);

  /**
   * Merge a prior context into this one.
   */
  void Merge(const SymbolicContext &that);

  /// Return all the frames used to execute a function.
  std::set<SymbolicFrame *> GetFrames(Func &func);
  /// Return the SCC version of a function.
  SCCFunction &GetSCCFunc(Func &func);

  /// Iterator over active frames.
  frame_iterator frame_begin()
  {
    return frame_iterator(activeFrames_.rbegin(), this);
  }
  frame_iterator frame_end()
  {
    return frame_iterator(activeFrames_.rend(), this);
  }
  llvm::iterator_range<frame_iterator> frames()
  {
    return llvm::make_range(frame_begin(), frame_end());
  }

  /// Iterator over objects.
  object_iterator object_begin() { return object_iterator(objects_.begin()); }
  object_iterator object_end() { return object_iterator(objects_.end()); }
  llvm::iterator_range<object_iterator> objects()
  {
    return llvm::make_range(object_begin(), object_end());
  }

private:
  /// Performs a store to an external pointer.
  bool StoreExtern(const Extern &e, const SymbolicValue &val, Type ty);
  /// Performs a store to an external pointer.
  bool StoreExtern(const Extern &e, int64_t off, const SymbolicValue &val, Type ty);
  /// Performs a load from an external pointer.
  SymbolicValue LoadExtern(const Extern &e, Type ty);
  /// Performs a load from an external pointer.
  SymbolicValue LoadExtern(const Extern &e, int64_t off, Type ty);
  /// Build a symbolic object from an object.
  SymbolicObject *BuildObject(ID<SymbolicObject> id, Object *object);

private:
  /// Reference to the heap.
  SymbolicHeap &heap_;
  /// Reference to the summary.
  SymbolicSummary &state_;

  /// Mapping from functions to their cached SCC representations.
  std::unordered_map<
      Func *,
      std::shared_ptr<SCCFunction>
  > funcs_;

  /// Mapping from heap-allocated objects to their symbolic values.
  ObjectMap objects_;

  /// Stack of frames.
  std::vector<SymbolicFrame> frames_;
  /// Stack of active frame IDs.
  std::vector<unsigned> activeFrames_;
  /// Over-approximate extern bucket.
  std::optional<SymbolicValue> extern_;
};
