// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <stack>
#include <queue>

#include "passes/tags/sat.h"



// -----------------------------------------------------------------------------
void SATProblem::Add(llvm::ArrayRef<ID<Lit>> pos, llvm::ArrayRef<ID<Lit>> neg)
{
  auto &clause = clauses_.emplace_back();
  for (auto lit : pos) {
    clause.push_back((static_cast<unsigned>(lit) << 1) | 0);
  }
  for (auto lit : neg) {
    clause.push_back((static_cast<unsigned>(lit) << 1) | 1);
  }
  if (clause.size() > 2) {
    is2SAT_ = false;
  }
}

// -----------------------------------------------------------------------------
bool SATProblem::IsSatisfiable()
{
  if (is2SAT_) {
    if (!solver2SAT_) {
      solver2SAT_.reset(new SAT2Solver(clauses_));
    }
    return solver2SAT_->IsSatisfiable();
  } else {
    if (!solverNSAT_) {
      solverNSAT_.reset(new SATNSolver(clauses_));
    }
    return solverNSAT_->IsSatisfiable();
  }
}

// -----------------------------------------------------------------------------
bool SATProblem::IsSatisfiableWith(ID<Lit> id)
{
  if (is2SAT_) {
    if (!solver2SAT_) {
      solver2SAT_.reset(new SAT2Solver(clauses_));
    }
    return solver2SAT_->IsSatisfiableWith(id);
  } else {
    if (!solverNSAT_) {
      solverNSAT_.reset(new SATNSolver(clauses_));
    }
    return solverNSAT_->IsSatisfiableWith(id);
  }
}

// -----------------------------------------------------------------------------
SATProblem::SAT2Solver::SAT2Solver(const ClauseList &list)
  : unsat_(false)
{
  struct Node {
    unsigned Index;
    unsigned Link;
    bool InComponent;
    std::set<unsigned> Next;

    Node() : Index(0), Link(0) {}
  };

  std::vector<Node> nodes;

  for (auto &clause : list) {
    switch (clause.size()) {
      case 1: {
        auto a = clause[0];
        auto size = std::max(a, a ^ 1) + 1;
        if (nodes.size() < size) {
          nodes.resize(size);
        }
        nodes[a ^ 1].Next.insert(a);
        break;
      }
      case 2: {
        auto a = clause[0], b = clause[1];
        auto size = std::max(std::max(a, b), std::max(a ^ 1, b ^ 1)) + 1;
        if (nodes.size() < size) {
          nodes.resize(size);
        }
        nodes[a ^ 1].Next.insert(b);
        nodes[b ^ 1].Next.insert(a);
        break;
      }
      default: {
        break;
      }
    }
  }

  unsigned index = 0;
  std::stack<unsigned> stack;
  std::function<void(unsigned)> visit = [&, this](unsigned nodeId)
  {
    auto &node = nodes[nodeId];
    node.Index = node.Link = ++index;
    node.InComponent = true;

    for (auto succId : node.Next) {
      if (nodes[succId].Index == 0) {
        visit(succId);
        node.Link = std::min(node.Link, nodes[succId].Link);
      } else if (!nodes[succId].InComponent) {
        node.Link = std::min(node.Link, nodes[succId].Link);
      }
    }

    if (node.Link == node.Index) {
      node.InComponent = true;

      unsigned sccId = sccGraph_.size();
      auto &scc = sccGraph_.emplace_back();
      std::vector<unsigned> inSCC;
      sccOfNode_.emplace(nodeId, sccId);
      inSCC.push_back(nodeId);
      while (!stack.empty() && nodes[stack.top()].Index > node.Link) {
        auto topId = stack.top();
        stack.pop();
        nodes[topId].InComponent = true;
        sccOfNode_.emplace(topId, sccId);
        inSCC.push_back(topId);
      }
      for (auto n : inSCC) {
        for (auto nextId : nodes[n].Next) {
          if (auto it = sccOfNode_.find(nextId); it != sccOfNode_.end()) {
            scc.insert(it->second);
          }
        }
      }
    } else {
      stack.push(nodeId);
    }
  };

  for (unsigned i = 0, n = nodes.size(); i < n; ++i) {
    if (nodes[i].Index == 0) {
      visit(i);
    }
  }

  for (auto [lit, scc] : sccOfNode_) {
    auto it = sccOfNode_.find(lit ^ 1);
    assert(it != sccOfNode_.end() && "missing component");
    if (scc == it->second) {
      unsat_ = true;
    }
  }
}

// -----------------------------------------------------------------------------
bool SATProblem::SAT2Solver::IsSatisfiable()
{
  return !unsat_;
}

// -----------------------------------------------------------------------------
bool SATProblem::SAT2Solver::IsSatisfiableWith(ID<Lit> id)
{
  // Add the constraint id \/ id, which implies an edge ~id -> id.
  // If this edge would introduce a cycle, the assignment is not satisfiable.
  // The presence of a cycle is indicated by a path from id to ~id.
  if (unsat_) {
    return false;
  }

  auto start = (static_cast<unsigned>(id) << 1) | 0;
  auto end = (static_cast<unsigned>(id) << 1) | 1;
  if (!sccOfNode_.count(start) || !sccOfNode_.count(end)) {
    return true;
  }

  auto startSCC = sccOfNode_[start];
  auto endSCC = sccOfNode_[end];

  std::set<unsigned> visited;
  std::queue<unsigned> q;
  q.push(startSCC);
  while (!q.empty()) {
    auto node = q.front();
    q.pop();
    if (node == endSCC) {
      return false;
    }
    if (!visited.insert(node).second) {
      continue;
    }
    for (auto next : sccGraph_[node]) {
      q.push(next);
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
void SATProblem::dump(llvm::raw_ostream &os)
{
  for (auto &clause : clauses_) {
    for (unsigned i = 0, n = clause.size(); i < n; ++i) {
      auto lit = clause[i];
      if (lit & 1) {
        os << "~";
      }
      os << (lit >> 1);
      if (i + 1 != n) {
        os << " \\/ ";
      }
    }
    os << "\n";
  }
}
