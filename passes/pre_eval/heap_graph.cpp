// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SCCIterator.h>

#include "passes/pre_eval/heap_graph.h"
#include "passes/pre_eval/symbolic_context.h"



// -----------------------------------------------------------------------------
HeapGraph::HeapGraph(SymbolicContext &ctx)
  : ctx_(ctx)
{
  for (auto &object : ctx.objects()) {
    llvm_unreachable("not implemented");
  }
  for (auto &alloc : ctx.allocs()) {
    llvm_unreachable("not implemented");
  }
  for (auto &frame : ctx.frames()) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
void HeapGraph::Extract(
    SymbolicValue &value,
    std::set<Func *> &funcs,
    BitSet<Node> &nodes)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void HeapGraph::Extract(
    Object *g,
    SymbolicValue &value,
    std::set<Func *> &funcs,
    BitSet<Node> &nodes)
{
  llvm_unreachable("not implemented");
}
