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
  Subset.Union(that.Subset);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Constraint::dump(llvm::raw_ostream &os)
{
  os << Id << "{" << Min << ", " << Max << "," << Subset << "}";
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
    if (c->Min == c->Max) {
      continue;
    }
    assert(c->Min < c->Max && "invalid constraint range");
  }

  RewriteTypes();
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

  auto node = [&nodes] (ID<Constraint> id) -> Node &
  {
    if (id >= nodes.size()) {
      nodes.resize(static_cast<unsigned>(id) + 1);
    }
    return nodes[id];
  };

  std::function<void(ID<Constraint>)> visit = [&,this](ID<Constraint> nodeId)
  {
    auto &nd = node(nodeId);
    nd.Index = index;
    nd.Link = index;
    nd.InComponent = false;
    ++index;

    auto *c = union_.Map(nodeId);
    for (auto succId : c->Subset) {
      if (succId >= nodes.size() || nodes[succId].Index == 0) {
        visit(succId);
        nodes[nodeId].Link = std::min(nodes[nodeId].Link, nodes[succId].Link);
      } else if (!nodes[succId].InComponent) {
        nodes[nodeId].Link = std::min(nodes[nodeId].Link, nodes[succId].Link);
      }
    }

    if (nodes[nodeId].Link == nodes[nodeId].Index) {
      nodes[nodeId].InComponent = true;

      BitSet<Constraint> &scc = sccs.emplace_back();
      scc.Insert(nodeId);
      while (!stack.empty() && nodes[stack.top()].Index > nodes[nodeId].Link) {
        auto topId = stack.top();
        stack.pop();
        nodes[topId].InComponent = true;
        scc.Insert(topId);
      }
    } else {
      stack.push(nodeId);
    }
  };

  for (auto *c : union_) {
    if (node(c->Id).Index == 0) {
      visit(c->Id);
    }
  }

  std::vector<ID<Constraint>> ids;
  for (const auto &scc : sccs) {
    auto base = *scc.begin();
    for (auto it = scc.begin(); it != scc.end(); ++it) {
      union_.Union(base, *it);
    }
    ids.push_back(union_.Find(base));
  }

  bool changed;
  do {
    changed = false;

    for (auto it = ids.begin(); it != ids.end(); ++it) {
      auto *to = union_.Map(*it);
      std::optional<std::pair<ConstraintType, ConstraintType>> in;
      for (auto pred : to->Subset) {
        auto *from = union_.Map(pred);
        if (to == from) {
          continue;
        }
        if (in) {
          in->first = GLB(in->first, from->Min);
          in->second = LUB(in->second, from->Max);
        } else {
          in.emplace(from->Min, from->Max);
        }
      }

      if (in) {
        if (to->Min <= in->first) {
          assert(in->first <= to->Max && "invalid lower bound");
          to->Min = in->first;
        } else {
          assert(in->first < to->Min && "invalid constraint");
        }
        if (in->second <= to->Max) {
          assert(to->Min <= in->second && "invalid upper bound");
          to->Max = in->second;
        } else {
          assert(to->Max < in->second && "invalid constraint");
        }
      }
    }

    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
      auto *to = union_.Map(*it);
      for (auto pred : to->Subset) {
        auto *from = union_.Map(pred);
        if (to == from) {
          continue;
        }
        if (from->Min <= to->Min) {
          assert(to->Min <= from->Max && "invalid lower bound");
          if (from->Min < to->Min) {
            from->Min = to->Min;
            changed = true;
          }
        } else {
          assert(to->Min < from->Min && "invalid constraint");
        }
        if (to->Max <= from->Max) {
          assert(from->Min <= to->Max && "invalid upper bound");
          if (to->Max < from->Max) {
            from->Max = to->Max;
            changed = true;
          }
        } else {
          assert(from->Max < to->Max && "invalid constraint");
        }
      }
    }
  } while (changed);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::RewriteTypes()
{
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          auto ref = inst.GetSubValue(i);
          auto ty = analysis_.Find(ref);
          auto *c = Map(ref);
          // TODO: rewrite types if a narrower bound is identified.
          // llvm::errs() << ty << " " << c->Min << " " << c->Max << "\n";
        }
      }
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
  assert(analysis_.Find(from) <= analysis_.Find(to) && "invalid subset");
  Map(to)->Subset.Insert(Find(from));
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
ConstraintType ConstraintSolver::UpperBound(Type ty, TaggedType type)
{
  switch (type.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      switch (ty) {
        case Type::V64: {
          return ConstraintType::HEAP_INT;
        }
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128: {
          if (target_->GetPointerType() == ty) {
            return ConstraintType::PTR_INT;
          } else {
            return ConstraintType::INT;
          }
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return ConstraintType::INT;
        }
      }
      llvm_unreachable("invalid type");
    }
    case TaggedType::Kind::INT: return ConstraintType::INT;
    case TaggedType::Kind::YOUNG: return ConstraintType::YOUNG;
    case TaggedType::Kind::HEAP_OFF: llvm_unreachable("not implemented");
    case TaggedType::Kind::HEAP: return ConstraintType::HEAP;
    case TaggedType::Kind::ADDR: return ConstraintType::ADDR;
    case TaggedType::Kind::ADDR_NULL: return ConstraintType::ADDR_INT;
    case TaggedType::Kind::ADDR_INT: return ConstraintType::ADDR_INT;
    case TaggedType::Kind::VAL: return ConstraintType::HEAP_INT;
    case TaggedType::Kind::FUNC: return ConstraintType::FUNC;
    case TaggedType::Kind::PTR: return ConstraintType::PTR;
    case TaggedType::Kind::PTR_NULL: return ConstraintType::PTR_INT;
    case TaggedType::Kind::PTR_INT: return ConstraintType::PTR_INT;
    case TaggedType::Kind::UNDEF: return ConstraintType::BOT;
  }
  llvm_unreachable("invalid type kind");
}

// -----------------------------------------------------------------------------
ConstraintType ConstraintSolver::LowerBound(Type ty, TaggedType type)
{
  switch (type.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      switch (ty) {
        case Type::V64: {
          return ConstraintType::BOT;
        }
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128: {
          if (target_->GetPointerType() == ty) {
            return ConstraintType::BOT;
          } else {
            return ConstraintType::INT;
          }
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          return ConstraintType::INT;
        }
      }
      llvm_unreachable("invalid type");
    }
    case TaggedType::Kind::INT: return ConstraintType::INT;
    case TaggedType::Kind::YOUNG: return ConstraintType::YOUNG;
    case TaggedType::Kind::HEAP_OFF: llvm_unreachable("not implemented");
    case TaggedType::Kind::HEAP: return ConstraintType::HEAP;
    case TaggedType::Kind::ADDR: return ConstraintType::HEAP;
    case TaggedType::Kind::ADDR_NULL: return ConstraintType::BOT;
    case TaggedType::Kind::ADDR_INT: return ConstraintType::BOT;
    case TaggedType::Kind::VAL: return ConstraintType::BOT;
    case TaggedType::Kind::FUNC: return ConstraintType::FUNC;
    case TaggedType::Kind::PTR: return ConstraintType::PTR_BOT;
    case TaggedType::Kind::PTR_NULL: return ConstraintType::BOT;
    case TaggedType::Kind::PTR_INT: return ConstraintType::BOT;
    case TaggedType::Kind::UNDEF: return ConstraintType::BOT;
  }
  llvm_unreachable("invalid type kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Infer(Ref<Inst> arg)
{
   auto ty = analysis_.Find(arg);
   AtLeastInfer(arg, ty);
   AtMostInfer(arg, ty);
}

