// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/PostOrderIterator.h>
#include <queue>
#include "core/cfg.h"



/// Analysis direction.
enum class Direction {
  FORWARD,
  BACKWARD,
};

/**
 * Kildall's algorithm for transfer functions with kill-gen sets.
 */
template <typename FlowSet, typename GenSet, typename KillSet, Direction Dir>
class KillGenSolver {
protected:
  struct InstInfo {
    /// Instruction which generates or clobbers.
    Inst *I;

    /// Liveness gen.
    GenSet Gen;
    /// Liveness kill.
    KillSet Kill;

    InstInfo(Inst *I) : I(I) {}
  };

public:
  /// Initialises the solver.
  KillGenSolver(Func &func);

  /// Builds and solves constraints, invokes callback.
  void Solve();

protected:
  /// Callback to generate constraints.
  virtual void Build(Inst &inst) = 0;

  /// Solve all the constraints.
  virtual void Traverse(Inst *inst, const FlowSet &set) = 0;

  /// Returns a kill-gen set for an instruction.
  InstInfo &Info(Inst *I)
  {
    return blocks_[blockToIndex_[I->getParent()]].Insts.emplace_back(I);
  }

private:
  /// Per-block information.
  struct BlockInfo {
    /// Block for which the info is generated.
    Block *B;
    /// Predecessor indices.
    llvm::SmallVector<uint64_t, 5> Preds;
    /// Successor indices.
    llvm::SmallVector<uint64_t, 5> Succs;
    /// Kill-gen of individual elements.
    std::vector<InstInfo> Insts;
    /// Gen of block.
    GenSet Gen;
    /// Kill of block.
    KillSet Kill;
    /// Per-block live in.
    FlowSet Flow;

    BlockInfo(Block *B) : B(B) {}
  };

protected:
  /// Reference to the function.
  Func &func_;

private:
  /// Block information for data flow analyses.
  std::vector<BlockInfo> blocks_;
  /// Mapping from blocks_ to indices.
  std::unordered_map<const Block *, uint64_t> blockToIndex_;
};

template <typename FlowSet, typename GenSet, typename KillSet, Direction Dir>
KillGenSolver<FlowSet, GenSet, KillSet, Dir>::KillGenSolver(Func &func)
  : func_(func)
{
  // Generate unique block IDs.
  for (Block &block : func) {
    blockToIndex_.insert({ &block, blocks_.size() });
    blocks_.push_back(&block);
  }

  // Build the graph.
  for (Block &block : func) {
    BlockInfo &blockInfo = blocks_[blockToIndex_[&block]];

    // Construct fast pred/succ information.
    for (const Block *pred : block.predecessors()) {
      blockInfo.Preds.push_back(blockToIndex_[pred]);
    }
    for (const Block *succ : block.successors()) {
      blockInfo.Succs.push_back(blockToIndex_[succ]);
    }
  }
}

template <typename FlowSet, typename GenSet, typename KillSet, Direction Dir>
void KillGenSolver<FlowSet, GenSet, KillSet, Dir>::Solve()
{
  // Build constraints.
  for (Block &block : func_) {
    for (Inst &inst : block) {
      Build(inst);
    }
  }

  // Construct per-block kill/gen.
  for (BlockInfo &block : blocks_) {
    auto Step = [&block](InstInfo &inst) {
      // kill' = kill U killNew
      block.Kill.Union(inst.Kill);
      // gen' = (gen - killNew) U genNew
      block.Gen.Minus(inst.Kill);
      block.Gen.Union(inst.Gen);
    };

    switch (Dir) {
      case Direction::FORWARD: {
        for (auto it = block.Insts.begin(); it != block.Insts.end(); ++it) {
          Step(*it);
        }
        break;
      }
      case Direction::BACKWARD: {
        for (auto it = block.Insts.rbegin(); it != block.Insts.rend(); ++it) {
          Step(*it);
        }
        break;
      }
    }
  }

  // Populate the worklist.
  std::queue<BlockInfo *> queue_;
  std::set<BlockInfo *> inQueue_;
  std::vector<llvm::GraphTraits<Func *>::NodeRef> blockOrder_;
  {
    auto Add = [&queue_, &inQueue_] (BlockInfo *info) {
      inQueue_.insert(info);
      queue_.push(info);
    };

    // Find the order of the nodes.
    auto Entry = llvm::GraphTraits<Func *>::getEntryNode(&func_);
    std::copy(po_begin(Entry), po_end(Entry), std::back_inserter(blockOrder_));

    // Add nodes to queue.
    switch (Dir) {
      case Direction::FORWARD: {
        for (auto it = blockOrder_.rbegin(); it != blockOrder_.rend(); ++it) {
          auto *block = &blocks_[blockToIndex_[*it]];

          std::optional<FlowSet> init;
          for (unsigned index : block->Preds) {
            auto *pred = &blocks_[index];

            FlowSet out = pred->Flow;
            out.Minus(pred->Kill);
            out.Union(pred->Gen);
            if (init) {
              init->Union(out);
            } else {
              init = out;
            }
          }

          if (init) {
            block->Flow = *init;
          }

          Add(block);
        }
        break;
      }
      case Direction::BACKWARD: {
        for (auto it = blockOrder_.begin(); it != blockOrder_.end(); ++it) {
          auto *block = &blocks_[blockToIndex_[*it]];

          std::optional<FlowSet> init;
          for (unsigned index : block->Preds) {
            auto *succ = &blocks_[index];

            FlowSet out = succ->Flow;
            out.Minus(succ->Kill);
            out.Union(succ->Gen);
            if (init) {
              init->Union(out);
            } else {
              init = out;
            }
          }

          if (init) {
            block->Flow = *init;
          }

          Add(block);
        }
        break;
      }
    }
  }

  // Iterate until worklist has elements.
  while (!queue_.empty()) {
    BlockInfo *block = queue_.front();
    queue_.pop();
    inQueue_.erase(block);

    /// Compute flow from preds/succs.
    FlowSet out = block->Flow;
    out.Minus(block->Kill);
    out.Union(block->Gen);

    // If inputs to a block change, continue.
    auto Transfer = [&out, &inQueue_, &queue_] (BlockInfo *next) {
      FlowSet in = next->Flow;
      in.Union(out);
      if (!(in == next->Flow)) {
        next->Flow = in;
        if (inQueue_.count(next) == 0) {
          queue_.push(next);
        }
      }
    };

    /// Traverse succs/preds.
    switch (Dir) {
      case Direction::FORWARD: {
        for (unsigned succ : block->Succs) {
          Transfer(&blocks_[succ]);
        }
        break;
      }
      case Direction::BACKWARD: {
        for (unsigned pred : block->Preds) {
          Transfer(&blocks_[pred]);
        }
        break;
      }
    }
  }

  // Traverse nodes, applying the transformation.
  for (Block *block : blockOrder_) {
    BlockInfo &info = blocks_[blockToIndex_[block]];

    FlowSet set = info.Flow;

    auto Step = [this, &set] (InstInfo &i) {
      set.Minus(i.Kill);
      set.Union(i.Gen);
      Traverse(i.I, set);
    };

    switch (Dir) {
      case Direction::FORWARD: {
        for (auto it = info.Insts.begin(); it != info.Insts.end(); ++it) {
          Step(*it);
        }
        break;
      }
      case Direction::BACKWARD: {
        for (auto it = info.Insts.rbegin(); it != info.Insts.rend(); ++it) {
          Step(*it);
        }
        break;
      }
    }
  }
}
