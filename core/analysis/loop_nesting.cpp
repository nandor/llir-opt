// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>
#include "core/block.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/analysis/loop_nesting.h"



// -----------------------------------------------------------------------------
LoopNesting::LoopNesting(const Func *func)
  : n_(func->size())
  , graph_(n_)
  , count_(0)
  , loopHeaders_(n_)
  , reducibleLoopHeaders_(n_)
  , lcaParents_(n_)
  , lcaVisited_(n_)
  , lcaAncestor_(n_)
  , irreducibleLoopHeader_(n_)
  , loopParent_(n_)
{
  // Set up the union-find structure.
  for (unsigned i = 0; i < n_; ++i) {
    auto &node = graph_[i];
    node.Link = i;
    node.Class = i;
    node.Rank = 0;
    node.LoopPtr = nullptr;
  }

  // Build the DFS tree.
  DFS(&func->getEntryBlock(), 0);

  // Remove cross-forward edges from the graph.
  bool hasCrossForwardEdges = false;
  for (unsigned x = 0; x < n_; ++x) {
    auto &preds = graph_[x].Pred;
    for (unsigned i = 0; i < preds.size(); ) {
      unsigned y = preds[i];
      if (IsForwardEdge(y, x) || IsCrossEdge(y, x)) {
        graph_[y].CrossForwardCandidates.push_back(x);
        preds[i] = preds.back();
        preds.pop_back();
        hasCrossForwardEdges = true;
      } else {
        ++i;
      }
    }
  }

  if (hasCrossForwardEdges) {
    LCA(0);
  }

  // Findloop in reverse-dfs order.
  for (int i = n_ - 1; i >= 0; --i) {
    for (auto [from, to] : graph_[i].CrossForwardEdges) {
      graph_[loopHeaders_.Find(to)].Pred.push_back(loopHeaders_.Find(from));
      MarkIrreducibleLoops(to);
    }
    FindLoop(i);
  }
}

// -----------------------------------------------------------------------------
bool LoopNesting::IsLoopEdge(const Block *a, const Block *b)
{
  if (a == b) {
    // Trivial self-loop.
    return true;
  } else {
    // A and B must be in the same loop and b must be the header.
    const unsigned idxA = blockToId_[a];
    if (auto parent = loopParent_[idxA]) {
      return parent == blockToId_[b];
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool LoopNesting::IsCrossEdge(unsigned a, unsigned b)
{
  const auto &nodeA = graph_[a];
  const auto &nodeB = graph_[b];
  return nodeA.Start > nodeB.Start && nodeA.End > nodeB.End;
}

// -----------------------------------------------------------------------------
bool LoopNesting::IsTreeEdge(unsigned a, unsigned b)
{
  const auto &nodeA = graph_[a];
  const auto &nodeB = graph_[b];
  return nodeA.Start < nodeB.Start && nodeA.End > nodeB.End;
}

// -----------------------------------------------------------------------------
bool LoopNesting::IsForwardEdge(unsigned a, unsigned b)
{
  return graph_[b].Parent != a && IsTreeEdge(a, b);
}

// -----------------------------------------------------------------------------
bool LoopNesting::IsBackEdge(unsigned a, unsigned b)
{
  const auto &nodeA = graph_[a];
  const auto &nodeB = graph_[b];
  return nodeA.Start > nodeB.Start && nodeA.End < nodeB.End;
}

// -----------------------------------------------------------------------------
void LoopNesting::FindLoop(unsigned header)
{
  llvm::SmallVector<unsigned, 16> loopBody;
  llvm::SmallVector<unsigned, 8> workList;
  llvm::DenseSet<unsigned> visited;

  bool isSelfLoop = false;
  for (auto pred : graph_[header].Pred) {
    isSelfLoop = isSelfLoop || pred == header;
    if (IsBackEdge(pred, header)) {
      if (auto node = loopHeaders_.Find(pred); node != header) {
        workList.push_back(node);
        visited.insert(node);
      }
    }
  }

  while (!workList.empty()) {
    auto node = workList.back();
    workList.pop_back();
    loopBody.push_back(node);

    for (auto pred : graph_[node].Pred) {
      if (!IsBackEdge(pred, node)) {
        auto group = loopHeaders_.Find(pred);
        if (group != header && !visited.count(group)) {
          workList.push_back(group);
          visited.insert(group);
        }
      }
    }
  }

  if (!loopBody.empty() || isSelfLoop) {
    const auto *block = graph_[header].BlockPtr;

    // Construct a loop node for the header.
    auto *loop = blockToLoop_.emplace(
        block,
        std::unique_ptr<Loop>(new Loop(block))
    ).first->second.get();

    roots_.insert(loop);

    // And inner nodes and loops to the new loop.
    for (auto node : loopBody) {
      loopParent_[node] = header;
      loopHeaders_.Union(node, header);

      auto *nodeBlock = graph_[node].BlockPtr;
      if (auto it = blockToLoop_.find(nodeBlock); it != blockToLoop_.end()) {
        auto *innerLoop = it->second.get();
        roots_.erase(innerLoop);
        loop->loops_.push_back(innerLoop);
      } else {
        loop->blocks_.push_back(nodeBlock);
      }
    }
  }
}

// -----------------------------------------------------------------------------
unsigned LoopNesting::DFS(const Block *block, unsigned parent)
{
  unsigned index = blockToId_.size();
  blockToId_.emplace(block, index);

  auto &node = graph_[index];
  node.Parent = parent;
  node.BlockPtr = block;
  node.Start = count_++;

  for (auto *succ : block->successors()) {
    unsigned id;
    if (auto it = blockToId_.find(succ); it == blockToId_.end()) {
      id = DFS(succ, index);
      graph_[index].Children.push_back(id);
    } else {
      id = it->second;
    }
    graph_[id].Pred.push_back(index);
  }

  node.End = count_++;
  return index;
}

// -----------------------------------------------------------------------------
void LoopNesting::LCA(unsigned node)
{
  lcaAncestor_[lcaParents_.Find(node)] = node;
  for (auto child : graph_[node].Children) {
    LCA(child);
    lcaParents_.Union(node, child);
    lcaAncestor_[lcaParents_.Find(node)] = node;
  }

  lcaVisited_[node] = true;
  for (unsigned end : graph_[node].CrossForwardCandidates) {
    if (lcaVisited_[end]) {
      auto lca = lcaAncestor_[lcaParents_.Find(end)];
      graph_[lca].CrossForwardEdges.emplace_back(node, end);
    }
  }
}

// -----------------------------------------------------------------------------
void LoopNesting::MarkIrreducibleLoops(unsigned node)
{
  std::optional<unsigned> parent = loopParent_[node];
  while (parent) {
    auto u = reducibleLoopHeaders_.Find(*parent);
    irreducibleLoopHeader_[u] = true;
    if ((parent = loopParent_[u])) {
      reducibleLoopHeaders_.Union(u, *parent);
    }
  }
}
