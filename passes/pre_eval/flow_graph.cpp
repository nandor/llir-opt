// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SCCIterator.h>

#include "core/call_graph.h"
#include "core/call_graph.h"
#include "passes/pre_eval/flow_graph.h"



// -----------------------------------------------------------------------------
FlowGraph::FlowGraph(Prog &prog)
{
  llvm::errs() << "Prog size: " << prog.size() << "\n";
  /*
  CallGraph graph(prog);
  std::set<std::set<const Func *>> loops;
  std::unordered_map<const Func *, std::set<const Func *>> subtrees;

  for (auto it = llvm::scc_begin(&graph); !it.isAtEnd(); ++it) {
    if (it->size() > 1 || (*it)[0]->IsRecursive()) {
      std::set<const Func *> set;
      for (const auto *node : *it) {
        if (auto *f = node->GetCaller()) {
          set.insert(f);
        }
      }
      loops.emplace(std::move(set));
    }

    std::set<const Func *> subtree;
    for (const auto *node :*it) {
      if (auto *f = node->GetCaller()) {
        auto it = subtrees.find(f);
        assert(it != subtrees.end() && "missing node");
        for (const Func *elem : it->second) {
          subtree.insert(elem);
        }
      }
    }
    subtrees.emplace()
  }
  */


}
