// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/dag.h"

#include <llvm/ADT/SCCIterator.h>

#include "core/cfg.h"
#include "core/inst.h"



// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, DAGBlock &node)
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
template <typename T>
static void InsertUnique(llvm::SmallVectorImpl<T> &vec, const T &elem)
{
  for (auto &e : vec) {
    if (e == elem) {
      return;
    }
  }
  vec.push_back(elem);
}

// -----------------------------------------------------------------------------
DAGFunc::DAGFunc(Func &func)
  : func_(func)
{
  for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
    auto *node = nodes_.emplace_back(
        std::make_unique<DAGBlock>(nodes_.size())
    ).get();

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
    node->Traps = false;
    node->IsReturn = false;
    node->IsRaise = false;
    node->IsTrap = false;

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
          node->IsReturn = true;
          node->Returns = true;
          break;
        }
        case Inst::Kind::TRAP: {
          node->IsTrap = true;
          node->Traps = true;
          break;
        }
        case Inst::Kind::RAISE: {
          node->IsRaise = true;
          node->Raises = true;
          break;
        }
      }

      for (Block *succ : block->successors()) {
        auto *succNode = blocks_[succ];
        if (succNode == node) {
          isLoop = true;
        } else {
          InsertUnique(node->Succs, succNode);
          InsertUnique(succNode->Preds, node);
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
      if (a->Returns == b->Returns) {
        if (a->Traps == b->Traps) {
          return a->Length > b->Length;
        } else {
          return !a->Traps;
        }
      } else {
        return a->Returns;
      }
    });
    succs.erase(std::unique(succs.begin(), succs.end()), succs.end());
  }
}
