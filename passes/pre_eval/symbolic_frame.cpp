// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/inst.h"
#include "core/cfg.h"
#include "core/dag.h"
#include "core/block.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_frame.h"
#include "passes/pre_eval/symbolic_object.h"
#include "passes/pre_eval/symbolic_summary.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    SymbolicSummary &state,
    DAGFunc &func,
    unsigned index,
    llvm::ArrayRef<SymbolicValue> args,
    llvm::ArrayRef<ID<SymbolicObject>> objects)
  : state_(state)
  , func_(&func)
  , index_(index)
  , valid_(true)
  , args_(args)
  , current_(&func.GetFunc().getEntryBlock())
{
  executed_.insert(current_);
  for (unsigned i = 0, n = objects.size(); i < n; ++i) {
    objects_.emplace(i, objects[i]);
  }
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    SymbolicSummary &state,
    unsigned index,
    llvm::ArrayRef<ID<SymbolicObject>> objects)
  : state_(state)
  , func_(nullptr)
  , index_(index)
  , valid_(true)
  , current_(nullptr)
{
  for (unsigned i = 0, n = objects.size(); i < n; ++i) {
    objects_.emplace(i, objects[i]);
  }
}

// -----------------------------------------------------------------------------
void SymbolicFrame::Leave()
{
  valid_ = false;
  current_ = nullptr;
  values_.clear();
  bypass_.clear();
  counts_.clear();
}

// -----------------------------------------------------------------------------
bool SymbolicFrame::Set(Ref<Inst> inst, const SymbolicValue &value)
{
  assert(inst->getParent()->getParent() == GetFunc() && "invalid set");

  state_.Map(inst, value);
  auto it = values_.emplace(inst, value);
  if (it.second) {
    return true;
  }
  auto &oldValue = it.first->second;
  if (oldValue == value) {
    return false;
  }
  oldValue = value;
  return true;
}

// -----------------------------------------------------------------------------
const SymbolicValue &SymbolicFrame::Find(ConstRef<Inst> inst)
{
  auto it = values_.find(inst);
  assert(it != values_.end() && "value not computed");
  return it->second;
}

// -----------------------------------------------------------------------------
const SymbolicValue *SymbolicFrame::FindOpt(ConstRef<Inst> inst)
{
  auto it = values_.find(inst);
  if (it == values_.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

// -----------------------------------------------------------------------------
ID<SymbolicObject> SymbolicFrame::GetObject(unsigned object)
{
  auto it = objects_.find(object);
  assert(it != objects_.end() && "object not in frame");
  return it->second;
}

// -----------------------------------------------------------------------------
void SymbolicFrame::Merge(const SymbolicFrame &that)
{
  assert(func_ == that.func_ && "mismatched functions");
  assert(index_ == that.index_ && "mismatched indices");

  for (auto &[id, value] : that.values_) {
    if (auto it = values_.find(id); it != values_.end()) {
      it->second.Merge(value);
    } else {
      values_.emplace(id, value);
    }
  }
}

// -----------------------------------------------------------------------------
bool SymbolicFrame::FindBypassed(
    std::set<DAGBlock *> &nodes,
    std::set<SymbolicContext *> &ctx,
    DAGBlock *start,
    DAGBlock *end)
{
  assert(valid_ && "frame was deactivated");

  if (auto it = bypass_.find(start); it != bypass_.end()) {
    nodes.insert(start);
    ctx.insert(it->second.get());
    return true;
  }
  if (start->Blocks.size() == 1 && executed_.count(*start->Blocks.begin())) {
    return false;
  }

  bool bypassed = false;
  for (DAGBlock *pred : start->Preds) {
    bypassed = FindBypassed(nodes, ctx, pred, start) || bypassed;
  }
  if (bypassed) {
    nodes.insert(start);
  }
  return bypassed;
}

// -----------------------------------------------------------------------------
SymbolicContext *SymbolicFrame::GetBypass(DAGBlock *node)
{
  auto it = bypass_.find(node);
  return it == bypass_.end() ? nullptr : &*it->second;
}

// -----------------------------------------------------------------------------
bool SymbolicFrame::Limited(Block *block)
{
  return counts_[block]++ > 256;
}

// -----------------------------------------------------------------------------
void SymbolicFrame::Continue(Block *node)
{
  executed_.insert(node);
  current_ = node;
}

// -----------------------------------------------------------------------------
void SymbolicFrame::Bypass(DAGBlock *node, const SymbolicContext &ctx)
{
  assert(valid_ && "frame was deactivated");

  auto it = bypass_.emplace(node, nullptr);
  if (it.second) {
    it.first->second = std::make_shared<SymbolicContext>(ctx);
  } else {
    it.first->second->Merge(ctx);
  }
}
