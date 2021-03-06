// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>

#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>

#include "core/dag.h"
#include "core/func.h"
#include "passes/pre_eval/symbolic_value.h"

class SymbolicContext;
class SymbolicFrameObject;
class SymbolicFrame;
class MemoryStoreInst;
class SymbolicSummary;



/**
 * Symbolic representation of the execution frame of a function.
 */
class SymbolicFrame {
public:
  /// Mapping from indices to frame objects.
  using ObjectMap = std::map<unsigned, ID<SymbolicObject>>;

  /// Iterator over objects.
  struct object_iterator : llvm::iterator_adaptor_base
      < object_iterator
      , ObjectMap::const_iterator
      , std::random_access_iterator_tag
      , ID<SymbolicObject>
      >
  {
    explicit object_iterator(ObjectMap::const_iterator it)
      : iterator_adaptor_base(it)
    {
    }

    ID<SymbolicObject> operator*() const { return this->I->second; }
    ID<SymbolicObject> operator->() const { return operator*(); }
  };

public:
  /// Create a new frame.
  SymbolicFrame(
      SymbolicSummary &ctx,
      DAGFunc &func,
      unsigned index,
      llvm::ArrayRef<SymbolicValue> args,
      llvm::ArrayRef<ID<SymbolicObject>> objects
  );

  /// Create a new top-level frame.
  SymbolicFrame(
      SymbolicSummary &ctx,
      unsigned index,
      llvm::ArrayRef<ID<SymbolicObject>> objects
  );

  /// Return the function.
  Func *GetFunc() { return func_ ? &func_->GetFunc() : nullptr; }
  /// Return the function.
  const Func *GetFunc() const
  {
    return const_cast<SymbolicFrame *>(this)->GetFunc();
  }

  /// Return the index.
  ID<SymbolicFrame> GetIndex() const { return index_; }

  /// De-activate the frame.
  void Leave();
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

  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return args_.size(); }

  /// Return the value of an argument.
  const SymbolicValue &Arg(unsigned index) { return args_[index]; }

  /// Return a specific object.
  ID<SymbolicObject> GetObject(unsigned object);

  /// Merges another frame into this one.
  void Merge(const SymbolicFrame &that);

  /**
   * Find the set of nodes and their originating contexts which reach
   * a join point after diverging on a bypassed path.
   */
  bool FindBypassed(
      std::set<DAGBlock *> &nodes,
      std::set<SymbolicContext *> &ctx,
      DAGBlock *start,
      DAGBlock *end
  );

  /// Find bypasses for a node pair.
  bool FindBypassed(
      std::set<DAGBlock *> &nodes,
      std::set<SymbolicContext *> &ctx,
      Block *start,
      Block *end)
  {
    return FindBypassed(nodes, ctx, GetNode(start), GetNode(end));
  }

  /// Return the bypassed context for the current node.
  SymbolicContext *GetBypass(DAGBlock *node);

  /// Return the current node.
  Block *GetCurrentBlock() const { return current_; }

  /// Return the node for a block.
  DAGBlock *GetNode(Block *block) { return (*func_)[block]; }

  /// Check whether the counter of a loop expired.
  bool Limited(Block *block);
  /// Enter a node for execution.
  void Continue(Block *node);

  /// Bypass a node.
  void Bypass(DAGBlock *node, const SymbolicContext &ctx);

  /// Check whether the node was bypassed.
  bool IsBypassed(Block *node) { return IsBypassed(GetNode(node)); }
  /// Check whether the node was bypassed.
  bool IsBypassed(DAGBlock *node) { return bypass_.find(node) != bypass_.end(); }
  /// Check whether the nodes was executed.
  bool IsExecuted(Block *block) { return executed_.count(block); }
  /// Mask a node as approximated.
  void Approximate(Block *block) { executed_.insert(block); }

  /// Find the node which contains a block.
  DAGBlock *Find(Block *block) { return (*func_)[block]; }

  /// Iterator over the nodes of the function.
  llvm::iterator_range<DAGFunc::node_iterator> nodes()
  {
    return llvm::make_range(func_->begin(), func_->end());
  }

  /// Iterator over objects.
  object_iterator object_begin() { return object_iterator(objects_.begin()); }
  object_iterator object_end() { return object_iterator(objects_.end()); }
  llvm::iterator_range<object_iterator> objects()
  {
    return llvm::make_range(object_begin(), object_end());
  }

private:
  /// Reference to the context.
  SymbolicSummary &state_;
  /// Reference to the function.
  DAGFunc *func_;
  /// Unique index for the frame.
  unsigned index_;
  /// Flag to indicate whether the index is valid.
  bool valid_;
  /// Arguments to the function.
  std::vector<SymbolicValue> args_;
  /// Mapping from object IDs to objects.
  ObjectMap objects_;
  /// Mapping from instructions to their symbolic values.
  std::unordered_map<ConstRef<Inst>, SymbolicValue> values_;
  /// Block being executed.
  Block *current_;
  /// Heap checkpoints at bypass points.
  std::unordered_map<DAGBlock *, std::shared_ptr<SymbolicContext>> bypass_;
  /// Set of nodes executed in this frame.
  std::set<Block *> executed_;
  /// Execution counts for nodes.
  std::unordered_map<Block *, unsigned> counts_;
};
