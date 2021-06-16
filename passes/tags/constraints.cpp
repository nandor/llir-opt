// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include "core/prog.h"
#include "passes/tags/constraints.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/register_analysis.h"

using namespace tags;


// -----------------------------------------------------------------------------
void ConstraintSolver::Constraint::Union(Constraint &that)
{
  if (that.Max <= Max) {
    Max = that.Max;
  } else {
    assert(Max <= that.Max && "invalid constraint");
  }
  if (Min <= that.Min) {
    Min = that.Min;
  } else {
    assert(that.Min <= Min && "invalid constraint");
  }
  Subsets.Union(that.Subsets);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Constraint::dump(llvm::raw_ostream &os)
{
  os << Id << "{" << Min << ", " << Max << "," << Subsets << "}";
}

// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver(
    RegisterAnalysis &analysis,
    const Target *target,
    Prog &prog)
  : analysis_(analysis)
  , target_(target)
  , prog_(prog)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Solve()
{
  BuildConstraints();
  CollapseEquivalences();
}

// -----------------------------------------------------------------------------
void ConstraintSolver::BuildConstraints()
{
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        Dispatch(inst);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::CollapseEquivalences()
{
  struct Node {
    unsigned Index;
    unsigned Link;
    bool InComponent;

    Node() : Index(0), Link(0), InComponent(false) {}
  };

  std::vector<Node> nodes;
  std::vector<BitSet<Constraint>> sccs;
  std::stack<ID<Constraint>> stack;

  unsigned index = 1;
  std::function<void(ID<Constraint>)> visit = [&,this](ID<Constraint> nodeId)
  {
    if (nodeId >= nodes.size()) {
      nodes.resize(static_cast<unsigned>(nodeId) + 1);
    }

    auto *node = &nodes[nodeId];
    node->Index = index;
    node->Link = index;
    node->InComponent = false;
    ++index;

    auto *c = union_.Map(nodeId);
    for (auto succId : c->Subsets) {
      if (succId >= nodes.size() || nodes[succId].Index == 0) {
        visit(succId);
        nodes[nodeId].Link = std::min(nodes[nodeId].Link, nodes[succId].Link);
      } else if (!nodes[succId].InComponent) {
        nodes[nodeId].Link = std::min(nodes[nodeId].Link, nodes[succId].Link);
      }
    }

    if (nodes[nodeId].Link == nodes[nodeId].Index) {
      nodes[nodeId].InComponent = true;

      BitSet<Constraint> *scc = nullptr;
      while (!stack.empty() && nodes[stack.top()].Index > nodes[nodeId].Link) {
        auto topId = stack.top();
        stack.pop();
        nodes[topId].InComponent = true;
        if (!scc) {
          scc = &sccs.emplace_back();
        }
        if (topId == 1) {
          abort();
        }
        scc->Insert(topId);
      }

      if (scc) {
        scc->Insert(nodeId);
      }
    } else {
      stack.push(nodeId);
    }
  };

  for (auto *node : union_) {
    nodes.resize(static_cast<unsigned>(node->Id) + 1);
    if (nodes[node->Id].Index == 0) {
      visit(node->Id);
    }
  }

  for (const auto &scc : sccs) {
    auto base = *scc.begin();
    for (auto it = scc.begin(); it != scc.end(); ++it) {
      union_.Union(base, *it);
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitInst(Inst &i)
{
  std::string msg;
  llvm::raw_string_ostream os(msg);
  os << i << "\n";
  llvm::report_fatal_error(msg.c_str());
}

// -----------------------------------------------------------------------------
ID<ConstraintSolver::Constraint> ConstraintSolver::Find(Ref<Inst> a)
{
  if (auto it = ids_.find(a); it != ids_.end()) {
    return union_.Find(it->second);
  } else {
    auto id = union_.Emplace();
    ids_.emplace(a, id);
    return id;
  }
}

// -----------------------------------------------------------------------------
ConstraintSolver::Constraint *ConstraintSolver::Map(Ref<Inst> a)
{
  return union_.Map(Find(a));
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Subset(Ref<Inst> from, Ref<Inst> to)
{
  Map(from)->Subsets.Insert(Find(to));
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtMost(Ref<Inst> a, const TaggedType &type)
{
  auto *c = Map(a);
  if (type <= c->Max) {
    c->Max = type;
  } else {
    assert(c->Max < type && "invalid constraint");
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtLeast(Ref<Inst> a, const TaggedType &type)
{
  auto *c = Map(a);
  if (c->Min <= type) {
    c->Min = type;
  } else {
    assert(type < c->Min && "invalid constraint");
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtMostInfer(Ref<Inst> arg)
{
  auto type = analysis_.Find(arg);
  if (type.IsUnknown()) {
    switch (auto ty = arg.GetType()) {
      case Type::V64: {
        return AtMost(arg, TaggedType::Val());
      }
      case Type::I8:
      case Type::I16:
      case Type::I32:
      case Type::I64:
      case Type::I128: {
        if (target_->GetPointerType() == ty) {
          return AtMost(arg, TaggedType::PtrInt());
        } else {
          return AtMost(arg, TaggedType::Int());
        }
      }
      case Type::F32:
      case Type::F64:
      case Type::F80:
      case Type::F128: {
        return AtMost(arg, TaggedType::Int());
      }
    }
    llvm_unreachable("invalid type");
  } else {
    return AtMost(arg, type);
  }
}
