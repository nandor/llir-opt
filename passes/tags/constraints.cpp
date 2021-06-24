// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include "core/prog.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"
#include "passes/tags/sat.h"
#include "passes/tags/tagged_type.h"

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
  for (auto def : that.Defs) {
    Defs.insert(def);
  }
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
  SolveConstraints();

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
static bool IsPolymorphic(const Inst &inst)
{
  return ::isa<ArgInst>(&inst) || ::isa<PhiInst>(&inst) ||
         ::isa<MovInst>(&inst) || ::isa<SelectInst>(&inst) ||
         ::isa<MemoryInst>(&inst) || isa<CallSite>(&inst);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::RewriteTypes()
{
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
    auto id = union_.Emplace(a);
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
  auto vto = analysis_.Find(to);
  auto vfrom = analysis_.Find(from);
  if (vfrom.IsUnknown() || vto.IsUnknown()) {
    return;
  }
  assert(vfrom <= vto && "invalid subset");
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

// -----------------------------------------------------------------------------
void ConstraintSolver::Alternatives(
    Ref<Inst> i,
    llvm::ArrayRef<Alternative> alternatives)
{
  using Group = llvm::SmallVector<Lit, 3>;

  llvm::SmallVector<Group, 3> groups;
  for (const auto &alt : alternatives) {
    auto &group = groups.emplace_back();
    group.emplace_back(alt.Disc);
    for (auto &[id, ip, tf] : alt.Conj) {
      group.emplace_back(id, ip, tf);
      group.emplace_back(id, !ip, !tf);
    }
  }

  Group g;
  std::function<void(int)> convert = [&](int n)
  {
    if (n == groups.size()) {
      conj_.push_back(g);
    } else {
      for (auto c : groups[n]) {
        g.push_back(c);
        convert(n + 1);
        g.pop_back();
      }
    }
  };
  convert(0);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::SolveConstraints()
{
  // Build constraints from types.
  BitSet<Constraint> ambiguous;
  for (auto *c : union_) {
    switch (c->Max) {
      case ConstraintType::BOT: {
        continue;
      }
      case ConstraintType::INT: {
        conj_.push_back({ IsInt(c->Id) });
        conj_.push_back({ NotPtr(c->Id) });
        continue;
      }
      case ConstraintType::PTR_BOT:
      case ConstraintType::YOUNG:
      case ConstraintType::HEAP:
      case ConstraintType::ADDR:
      case ConstraintType::PTR:
      case ConstraintType::FUNC: {
        conj_.push_back({ IsPtr(c->Id) });
        conj_.push_back({ NotInt(c->Id) });
        continue;
      }
      case ConstraintType::ADDR_INT:
      case ConstraintType::PTR_INT:
      case ConstraintType::HEAP_INT: {
        conj_.push_back({ IsInt(c->Id), IsPtr(c->Id) });
        ambiguous.Insert(c->Id);
        continue;
      }
    }
    llvm_unreachable("invalid constraint kind");
  }

  // Build subset constraints.
  for (auto *c : union_) {
    for (auto sub : c->Subset) {
      // exists p = int -> c = int <=> forall p, (p <> int \/ c = int)
      // exists p = ptr -> c = ptr <=> forlal p, (p <> ptr \/ c = ptr)
      conj_.push_back({ NotInt(sub), IsInt(c->Id) });
      conj_.push_back({ NotPtr(sub), IsPtr(c->Id) });
    }
  }

  // Eliminate trivial redundancies due to unification.
  std::unordered_set<std::vector<Lit>> dedup;
  {
    for (unsigned i = 0; i < conj_.size(); ) {
      std::set<Lit> terms;
      for (auto [id, ip, tf] : conj_[i]) {
        terms.emplace(union_.Find(id), ip, tf);
      }
      if (dedup.emplace(terms.begin(), terms.end()).second) {
        conj_[i++].assign(terms.begin(), terms.end());
      } else {
        std::swap(conj_[i], *conj_.rbegin());
        conj_.pop_back();
      }
    }
  }

  // Find trivially true clauses and eliminate redundancies.
  BitSet<Constraint> isPtr, isInt, notPtr, notInt;
  {
    std::unordered_set<Lit> trues;
    bool changed;
    do {
      changed = false;

      // Register true clauses, remove them from the set of conjunctions.
      for (unsigned i = 0; i < conj_.size(); ) {
        if (conj_[i].size() == 1) {
          trues.insert(conj_[i][0]);
          std::swap(conj_[i], *conj_.rbegin());
          conj_.pop_back();
        } else {
          ++i;
        }
      }

      // Eliminate clauses with at least one lement known to be true.
      for (unsigned i = 0; i < conj_.size(); ) {
        llvm::SmallVector<Lit, 4> lits;

        bool hasTrueLiteral = false;
        for (auto &lit : conj_[i]) {
          if (trues.count(lit)) {
            hasTrueLiteral = true;
            break;
          } else {
            auto [id, intOrPtr, negated] = lit;
            if (!trues.count({ id, intOrPtr, !negated })) {
              lits.push_back(lit);
            } else {
              changed = true;
            }
          }
        }

        if (hasTrueLiteral) {
          std::swap(conj_[i], *conj_.rbegin());
          conj_.pop_back();
        } else {
          assert(!lits.empty() && "false constraint");
          conj_[i++].assign(lits.begin(), lits.end());
        }
      }
      // (A \/ B) /\ (A \/ ~B) implies A
      for (auto &conj : conj_) {
        if (conj.size() != 2) {
          continue;
        }
        auto a = conj[0], b = conj[1];
        if (dedup.count({ a, Conj(b) })) {
          trues.insert(a);
          changed = true;
        }
        if (dedup.count({ Conj(a), b })) {
          trues.insert(b);
          changed = true;
        }
      }
    } while (changed);

    // Save the truth/falsity information.
    for (auto [id, intOrPtr, trueOrFalse] : trues) {
      if (intOrPtr) {
        if (trueOrFalse) {
          isInt.Insert(id);
        } else {
          notInt.Insert(id);
        }
      } else {
        if (trueOrFalse) {
          isPtr.Insert(id);
        } else {
          notPtr.Insert(id);
        }
      }
    }
  }

  // Find groups of independent constraints.
  class Problem {
  public:
    void Add(llvm::ArrayRef<Lit> conj)
    {
      BitSet<SATProblem::Lit> pos, neg;
      for (auto lit : conj) {
        auto id = Map(lit);
        if (std::get<2>(lit)) {
          pos.Insert(id);
        } else {
          neg.Insert(id);
        }
      }
      p_.Add(pos, neg);
    }

    bool IsSatisfiable() { return p_.IsSatisfiable(); }

    bool IsSatisfiableWith(const Lit &lit)
    {
      return p_.IsSatisfiableWith(Map(lit));
    }


  private:
    ID<SATProblem::Lit> Map(const Lit &lit)
    {
      auto key = std::make_pair(std::get<0>(lit), std::get<1>(lit));
      return lits_.emplace(key, lits_.size()).first->second;
    }

  private:
    SATProblem p_;

    std::unordered_map
      < std::pair<ID<Constraint>, bool>
      , ID<SATProblem::Lit>
      > lits_;
  };

  std::vector<Problem> problems;
  std::unordered_map<ID<Constraint>, ID<Problem>> problemIDs;
  {
    struct Group {
      Group(ID<Group> id) {}
      void Union(const Group &that) {}
    };
    UnionFind<Group> groups;
    std::unordered_map<ID<Constraint>, ID<Group>> idToGroup;
    std::unordered_map<ID<Group>, ID<Problem>> groupToProblem;

    auto find = [&] (ID<Constraint> id)
    {
      if (auto it = idToGroup.find(id); it != idToGroup.end()) {
        return groups.Find(it->second);
      } else {
        return idToGroup.emplace(id, groups.Emplace()).first->second;
      }
    };

    for (auto &conj : conj_) {
      auto base = std::get<0>(conj[0]);
      for (auto [id, intOrPtr, trueOrFalse] : conj) {
        groups.Union(find(base), find(id));
      }
    }

    for (auto &conj : conj_) {
      auto id = std::get<0>(conj[0]);
      auto it = idToGroup.find(id);
      assert(it != idToGroup.end() && "invalid mapping");
      auto group = groups.Find(it->second);

      auto jt = groupToProblem.find(group);
      Problem *p;
      if (jt == groupToProblem.end()) {
        auto pid = problems.size();
        groupToProblem.emplace(group, pid);
        p = &problems.emplace_back();
      } else {
        auto pid = jt->second;
        p = &problems[pid];
      }

      p->Add(conj);
    }

    for (auto [cid, gid] : idToGroup) {
      auto it = idToGroup.find(cid);
      assert(it != idToGroup.end() && "invalid mapping");
      auto jt = groupToProblem.find(groups.Find(it->second));
      assert(jt != groupToProblem.end() && "invalid mapping");
      problemIDs.emplace(cid, jt->second);
    }
  }

  // Ensure all constraint systems are satisfiable.
  #ifndef NDEBUG
  for (auto &p : problems) {
    assert(p.IsSatisfiable() && "system not satisfiable");
  }
  #endif

  for (auto id : ambiguous) {
    if (isInt.Contains(id) && notPtr.Contains(id)) {
      llvm_unreachable("not implemented");
      continue;
    }
    if (isPtr.Contains(id) && notPtr.Contains(id)) {
      llvm_unreachable("not implemented");
      continue;
    }

    if (auto it = problemIDs.find(id); it != problemIDs.end()) {
      auto &p = problems[it->second];

      if (!p.IsSatisfiableWith(IsInt(id))) {
        llvm_unreachable("not implemented");
        continue;
      }
      if (!p.IsSatisfiableWith(IsPtr(id))) {
        llvm_unreachable("not implemented");
        continue;
      }
    }
  }
}
