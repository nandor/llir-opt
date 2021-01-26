// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/inst.h"
#include "core/cfg.h"
#include "core/block.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_frame.h"
#include "passes/pre_eval/symbolic_object.h"
#include "passes/pre_eval/symbolic_summary.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, SCCNode &node)
{
  bool first = true;
  for (Block *block :node.Blocks) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << block->getName();
  }
  return os;
}

// -----------------------------------------------------------------------------
SCCFunction::SCCFunction(Func &func)
  : func_(func)
{
  for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
    auto *node = nodes_.emplace_back(std::make_unique<SCCNode>()).get();

    unsigned size = 0;
    for (Block *block : *it) {
      node->Blocks.insert(block);
      blocks_.emplace(block, node);
      size += block->size();
    }

    // Connect to other nodes & determine whether node is a loop.
    node->Length = size;
    node->Returns = false;
    node->Lands = false;

    bool isLoop = it->size() > 1;
    for (Block *block : *it) {
      for (auto &inst : *block) {
        if (inst.Is(Inst::Kind::LANDING_PAD)) {
          node->Lands = true;
        }
      }
      auto *term = block->GetTerminator();
      switch (term->GetKind()) {
        default: llvm_unreachable("not a terminator");
        case Inst::Kind::JUMP:
        case Inst::Kind::JUMP_COND:
        case Inst::Kind::SWITCH:
        case Inst::Kind::CALL:
        case Inst::Kind::INVOKE: {
          break;
        }
        case Inst::Kind::RETURN:
        case Inst::Kind::TAIL_CALL: {
          node->Returns = true;
          break;
        }
        case Inst::Kind::TRAP: {
          node->Traps = true;
          break;
        }
        case Inst::Kind::RAISE: {
          node->Raises = true;
          break;
        }
      }

      for (Block *succ : block->successors()) {
        auto *succNode = blocks_[succ];
        if (succNode == node) {
          isLoop = true;
        } else {
          node->Succs.push_back(succNode);
          succNode->Preds.insert(node);
          node->Length = std::max(
              node->Length,
              succNode->Length + size
          );
          node->Returns = node->Returns || succNode->Returns;
        }
      }
    }
    node->IsLoop = isLoop;

    // Sort successors by their length.
    auto &succs = node->Succs;
    std::sort(succs.begin(), succs.end(), [](auto *a, auto *b) {
      if (a->Lands == b->Lands) {
        if (a->Returns == b->Returns) {
          return a->Length > b->Length;
        } else {
          return a->Returns;
        }
      } else {
        return !a->Lands;
      }
    });
    succs.erase(std::unique(succs.begin(), succs.end()), succs.end());
  }
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    SymbolicSummary &state,
    SCCFunction &func,
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
    std::set<SCCNode *> &nodes,
    std::set<SymbolicContext *> &ctx,
    SCCNode *start,
    SCCNode *end)
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
  for (SCCNode *pred : start->Preds) {
    bypassed = FindBypassed(nodes, ctx, pred, start) || bypassed;
  }
  if (bypassed) {
    nodes.insert(start);
  }
  return bypassed;
}

// -----------------------------------------------------------------------------
SymbolicContext *SymbolicFrame::GetBypass(SCCNode *node)
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
void SymbolicFrame::Bypass(SCCNode *node, const SymbolicContext &ctx)
{
  assert(valid_ && "frame was deactivated");

  auto it = bypass_.emplace(node, nullptr);
  if (it.second) {
    it.first->second = std::make_shared<SymbolicContext>(ctx);
  } else {
    it.first->second->Merge(ctx);
  }
}
