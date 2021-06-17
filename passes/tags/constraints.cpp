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
bool operator<(ConstraintType a, ConstraintType b)
{
  switch (a) {
    case ConstraintType::BOT: {
      return b != ConstraintType::BOT;
    }
    case ConstraintType::INT: {
      return b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR_INT ||
             b == ConstraintType::VAL;
    }
    case ConstraintType::VAL: {
      return b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR_INT;
    }
    case ConstraintType::HEAP: {
      return b == ConstraintType::VAL ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR ||
             b == ConstraintType::ADDR_INT;
    }
    case ConstraintType::PTR_BOT: {
      return b == ConstraintType::HEAP ||
             b == ConstraintType::VAL ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR ||
             b == ConstraintType::ADDR_INT ||
             b == ConstraintType::FUNC;
    }
    case ConstraintType::YOUNG: {
      return b == ConstraintType::HEAP ||
             b == ConstraintType::VAL ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT;
    }
    case ConstraintType::FUNC: {
      return b == ConstraintType::HEAP ||
             b == ConstraintType::VAL ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT;
    }
    case ConstraintType::PTR: {
      return b == ConstraintType::PTR_INT;
    }
    case ConstraintType::PTR_INT: {
      return false;
    }
    case ConstraintType::ADDR: {
      return b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR_INT;
    }
    case ConstraintType::ADDR_INT: {
      return b == ConstraintType::PTR_INT;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, ConstraintType type)
{
  switch (type) {
    case ConstraintType::BOT: os << "bot"; return os;
    case ConstraintType::INT: os << "int"; return os;
    case ConstraintType::PTR_BOT: os << "ptr_bot"; return os;
    case ConstraintType::YOUNG: os << "young"; return os;
    case ConstraintType::HEAP: os << "heap"; return os;
    case ConstraintType::ADDR: os << "addr"; return os;
    case ConstraintType::PTR: os << "ptr"; return os;
    case ConstraintType::ADDR_INT: os << "addr|int"; return os;
    case ConstraintType::PTR_INT: os << "ptr|int"; return os;
    case ConstraintType::VAL: os << "val"; return os;
    case ConstraintType::FUNC: os << "func"; return os;
  }
  llvm_unreachable("invalid constraint kind");
}



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

  for (auto *c : union_) {
    assert(c->Min <= c->Max && "invalid constraint range");
  }
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
void ConstraintSolver::AtMost(Ref<Inst> a, ConstraintType type)
{
  auto *c = Map(a);
  if (type <= c->Max) {
    assert(c->Min <= type && "invalid upper bound");
    c->Max = type;
  } else {
    assert(c->Max < type && "invalid constraint");
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtLeast(Ref<Inst> a, ConstraintType type)
{
  auto *c = Map(a);
  if (c->Min <= type) {
    assert(type <= c->Max && "invalid lower bound");
    c->Min = type;
  } else {
    assert(type < c->Min && "invalid constraint");
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtMostInfer(Ref<Inst> arg)
{
  auto type = analysis_.Find(arg);
  switch (type.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      switch (auto ty = arg.GetType()) {
        case Type::V64: {
          return AtMost(arg, ConstraintType::VAL);
        }
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128: {
          if (target_->GetPointerType() == ty) {
            return AtMost(arg, ConstraintType::PTR_INT);
          } else {
            return AtMost(arg, ConstraintType::INT);
          }
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return AtMost(arg, ConstraintType::INT);
        }
      }
      llvm_unreachable("invalid type");
    }

    case TaggedType::Kind::INT: return AtMost(arg, ConstraintType::INT);
    case TaggedType::Kind::YOUNG: return AtMost(arg, ConstraintType::YOUNG);
    case TaggedType::Kind::HEAP_OFF: llvm_unreachable("not implemented");
    case TaggedType::Kind::HEAP: return AtMost(arg, ConstraintType::HEAP);
    case TaggedType::Kind::ADDR: return AtMost(arg, ConstraintType::ADDR);
    case TaggedType::Kind::ADDR_NULL: return AtMost(arg, ConstraintType::ADDR_INT);
    case TaggedType::Kind::ADDR_INT: return AtMost(arg, ConstraintType::ADDR_INT);
    case TaggedType::Kind::VAL: return AtMost(arg, ConstraintType::VAL);
    case TaggedType::Kind::FUNC: return AtMost(arg, ConstraintType::FUNC);
    case TaggedType::Kind::PTR: return AtMost(arg, ConstraintType::PTR);
    case TaggedType::Kind::PTR_NULL: return AtMost(arg, ConstraintType::PTR_INT);
    case TaggedType::Kind::PTR_INT: return AtMost(arg, ConstraintType::PTR_INT);
    case TaggedType::Kind::UNDEF: return AtMost(arg, ConstraintType::BOT);
  }
  llvm_unreachable("invalid type kind");
}
