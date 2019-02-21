// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/raw_ostream.h>

#include "core/block.h"
#include "core/func.h"
#include "passes/global_data_elim/constraint.h"
#include "passes/global_data_elim/solver.h"



/**
 * Vector which keeps a single copy of each element.
 */
template<typename T>
class SetQueue {
public:
  SetQueue()
  {
  }

  bool empty() const
  {
    return queue_.empty();
  }

  size_t size() const
  {
    return queue_.size();
  }

  T pop()
  {
    T v = queue_.back();
    queue_.pop_back();
    set_.erase(v);
    return v;
  }

  void push(T V)
  {
    if (set_.count(V)) {
      return;
    }
    queue_.push_back(V);
    set_.insert(V);
  }

private:
  std::unordered_set<T> set_;
  std::vector<T> queue_;
};



// -----------------------------------------------------------------------------
void ConstraintSolver::Iterate()
{
  SetQueue<Constraint *> queue;

  for (auto &node : fixed_) {
    if (node.Is(Constraint::Kind::PTR)) {
      queue.push(&node);
      continue;
    }
  }

  // Propagate values from sets: this is guaranteed to converge.
  while (!queue.empty()) {
    Constraint *c = queue.pop();
    bool propagate = false;

    // Evaluate the constraint, updating the rule node.
    switch (c->GetKind()) {
      case Constraint::Kind::PTR: {
        propagate = !static_cast<CPtr *>(c)->IsEmpty();
        break;
      }
      case Constraint::Kind::SUBSET: {
        auto *csubset = static_cast<CSubset *>(c);
        auto *from = Lookup(csubset->GetSubset());
        auto *to = Lookup(csubset->GetSet());

        from->ForEach([&propagate, to](auto &item) {
          propagate |= to->Store(item);
        });

        auto *set = csubset->GetSet();
        if (propagate) {
          queue.push(set);
          propagate = false;
        }
        break;
      }
      case Constraint::Kind::UNION: {
        auto *cunion = static_cast<CUnion *>(c);
        auto *lhs = Lookup(cunion->GetLHS());
        auto *rhs = Lookup(cunion->GetRHS());
        auto *to = Lookup(cunion);

        lhs->ForEach([to, &propagate](auto &item) {
          propagate |= to->Store(item);
        });
        rhs->ForEach([to, &propagate](auto &item) {
          propagate |= to->Store(item);
        });

        //if (propagate) { Dump(c); }

        break;
      }
      case Constraint::Kind::OFFSET: {
        auto *coffset = static_cast<COffset *>(c);
        auto *from = Lookup(coffset->GetPointer());
        auto *to = Lookup(coffset);

        from->ForEach([&propagate, coffset, to](auto &item) {
          if (auto newItem = item.Offset(coffset->GetOffset())) {
            propagate |= to->Store(*newItem);
          }
        });
        //if (propagate) { Dump(c); }

        break;
      }
      case Constraint::Kind::LOAD: {
        auto *cload = static_cast<CLoad *>(c);
        auto *from = Lookup(cload->GetPointer());
        auto *to = Lookup(cload);

        from->ForEach([&propagate, to, cload, this](auto &item) {
          if (auto node = item.GetNode()) {
            loads_[node->first].insert(cload);
          }
          item.Load([&propagate, to](auto &item) {
            propagate |= to->Store(item);
          });
        });
        //if (propagate) { Dump(c); }

        break;
      }
      case Constraint::Kind::STORE: {
        auto *cstore = static_cast<CStore *>(c);
        auto *from = Lookup(cstore->GetValue());
        auto *to = Lookup(cstore->GetPointer());

        bool changed = false;
        from->ForEach([&changed, &queue, to, this](auto &fromItem) {
          to->ForEach([&changed, &queue, &fromItem, this](auto &toItem) {
            if (toItem.Store(fromItem)) {
              if (auto node = toItem.GetNode()) {
                for (auto *load : loads_[node->first]) {
                  queue.push(load);
                }
              }
              changed = true;
            }
          });
        });

        //if (changed) { Dump(c); }

        break;
      }
      case Constraint::Kind::CALL: {
        propagate = true;
        break;
      }
    }

    // If the set of the node changed, propagate it forward to other nodes.
    if (propagate) {
      for (auto *user : c->users()) {
        if (user->Is(Constraint::Kind::SUBSET)) {
          auto *csubset = static_cast<CSubset *>(user);
          if (csubset->GetSubset() != c) {
            continue;
          }
        }

        if (user->Is(Constraint::Kind::STORE)) {
          auto *cstore = static_cast<CStore *>(user);
          if (cstore->GetValue() != c) {
            continue;
          }
        }

        queue.push(user);
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::vector<Func *> ConstraintSolver::Expand()
{
  Iterate();

  std::vector<Func *> callees;
  for (auto &node : fixed_) {
    if (!node.Is(Constraint::Kind::CALL)) {
      continue;
    }

    auto &call = static_cast<CCall &>(node);
    auto *bag = Lookup(call.GetCallee());
    bag->ForEach([&callees, &call, this](auto &item) {
      if (auto *func = item.GetFunc()) {
        auto &expanded = expanded_[&call];
        if (!expanded.insert(func).second) {
          return;
        }
        if (std::find(callees.begin(), callees.end(), func) == callees.end()) {
          callees.push_back(func);
        }
        llvm::errs() << "EXPAND_INDIRECT: " << &call << " " << func->getName() << "\n";

        // Connect arguments and return value.
        auto &funcSet = this->operator[](func);
        for (unsigned i = 0; i < call.GetNumArgs(); ++i) {
          if (auto *arg = call.GetArg(i)) {
            if (i >= funcSet.Args.size()) {
              if (func->IsVarArg()) {
                Subset(arg, funcSet.VA);
              }
            } else {
              Subset(arg, funcSet.Args[i]);
            }
          }
        }
        Subset(funcSet.Return, &call);

        Progress();
      }
      if (auto *ext = item.GetExtern()) {
        assert(!"not implemented");
      }
    });
  }

  return callees;
}


// -----------------------------------------------------------------------------
void ConstraintSolver::Dump(const Bag::Item &item)
{
  auto &os = llvm::errs();
  if (auto *func = item.GetFunc()) {
    os << func->getName();
  }
  if (auto *ext = item.GetExtern()) {
    os << ext->getName();
  }
  if (auto node = item.GetNode()) {
    os << node->first;
    if (node->second) {
      os << "+" << *node->second;
    } else {
      os << "+inf";
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Dump(class Bag *bag)
{
  bool needsComma = false;
  bag->ForEach([&needsComma, this](auto &item) {
    if (needsComma) llvm::errs() << ", "; needsComma = true;
    Dump(item);
  });
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Dump(const Constraint *c)
{
  auto &os = llvm::errs();
  switch (c->GetKind()) {
    case Constraint::Kind::PTR: {
      auto *cptr = static_cast<const CPtr *>(c);
      os << c << " = ptr{";
      Dump(cptr->GetBag());
      os << "}\n";
      break;
    }
    case Constraint::Kind::SUBSET: {
      auto *csubset = static_cast<const CSubset *>(c);
      os << "subset(";
      os << csubset->GetSubset() << ", " << csubset->GetSet();
      os << ")\n";
      break;
    }
    case Constraint::Kind::UNION: {
      auto *cunion = static_cast<const CUnion *>(c);
      os << c << " = union(";
      os << cunion->GetLHS() << ", " << cunion->GetRHS();
      os << ")\n";
      break;
    }
    case Constraint::Kind::OFFSET: {
      auto *coffset = static_cast<const COffset *>(c);
      os << c << " = offset(";
      os << coffset->GetPointer() << ", ";
      if (auto off = coffset->GetOffset()) {
        os << *off;
      } else {
        os << "inf";
      }
      os << ")\n";
      break;
    }
    case Constraint::Kind::LOAD: {
      auto *cload = static_cast<const CLoad *>(c);
      os << c << " = load(" << cload->GetPointer() << ")\n";
      break;
    }
    case Constraint::Kind::STORE: {
      auto *cstore = static_cast<const CStore *>(c);
      os << "store(";
      os << cstore->GetValue() << ", " << cstore->GetPointer();
      os << ")\n";
      break;
    }
    case Constraint::Kind::CALL: {
      auto *ccall = static_cast<const CCall *>(c);
      os << c << " = call(";
      os << ccall->GetCallee();
      for (unsigned i = 0; i < ccall->GetNumArgs(); ++i) {
        os << ", " << ccall->GetArg(i);
      }
      os << ")\n";
      break;
    }
  }
}
