// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/inst.h"
#include "core/cfg.h"
#include "core/block.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_object.h"
#include "passes/pre_eval/symbolic_frame.h"

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
      if (block->GetTerminator()->IsReturn()) {
        node->Returns = true;
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
SCCNode *SCCFunction::GetEntryNode()
{
  return blocks_[&func_.getEntryBlock()];
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    SCCFunction &func,
    unsigned index,
    llvm::ArrayRef<SymbolicValue> args)
  : func_(&func)
  , index_(index)
  , valid_(true)
  , args_(args)
  , current_(func.GetEntryNode())
{
  Initialise(func.GetFunc().objects());
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(
    unsigned index,
    llvm::ArrayRef<Func::StackObject> objects)
  : func_(nullptr)
  , index_(index)
  , valid_(true)
  , current_(nullptr)
  , previous_(nullptr)
{
  Initialise(objects);
}

// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(const SymbolicFrame &that)
  : func_(that.func_)
  , index_(that.index_)
  , valid_(that.valid_)
  , args_(that.args_)
  , values_(that.values_)
  , current_(that.current_)
  , previous_(that.previous_)
{
  for (auto &[id, object] : that.objects_) {
    objects_.emplace(id, std::make_unique<SymbolicFrameObject>(*this, *object));
  }
}

// -----------------------------------------------------------------------------
bool SymbolicFrame::Set(Ref<Inst> i, const SymbolicValue &value)
{
  auto it = values_.emplace(i, value);
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
void SymbolicFrame::Initialise(llvm::ArrayRef<Func::StackObject> objects)
{
  for (auto &object : objects) {
    objects_.emplace(
        object.Index,
        std::make_unique<SymbolicFrameObject>(
            *this,
            object.Index,
            object.Size,
            object.Alignment
        )
    );
  }
}

// -----------------------------------------------------------------------------
void SymbolicFrame::LUB(const SymbolicFrame &that)
{
  for (auto &[id, object] : that.objects_) {
    if (auto it = objects_.find(id); it != objects_.end()) {
      it->second->LUB(*object);
    } else {
      llvm_unreachable("not implemented");
    }
  }
  for (auto &[id, value] : that.values_) {
    if (auto it = values_.find(id); it != values_.end()) {
      it->second.LUB(value);
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
  if (auto it = bypass_.find(start); it != bypass_.end()) {
    nodes.insert(start);
    ctx.insert(it->second.get());
    return true;
  }
  if (executed_.count(start)) {
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
void SymbolicFrame::Bypass(SCCNode *node, const SymbolicContext &ctx)
{
  bypass_.emplace(node, std::make_unique<SymbolicContext>(ctx));
}
