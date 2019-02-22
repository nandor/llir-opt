// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>
#include <unordered_set>

class Constraint;



// -----------------------------------------------------------------------------
class ConstraintSolver final {
public:
  struct FuncSet {
    /// Argument sets.
    std::vector<Constraint *> Args;
    /// Return set.
    Constraint *Return;
    /// Frame of the function.
    Constraint *Frame;
    /// Variable argument glob.
    Constraint *VA;
    /// True if function was expanded.
    bool Expanded;
  };

public:
  ConstraintSolver()
    : extern_(Ptr(Bag()))
  {
  }

  /// Creates a store constraint.
  Constraint *Store(Constraint *ptr, Constraint *val)
  {
    auto it = dedupStore_.emplace(std::make_pair(val, ptr), nullptr);
    if (it.second) {
      it.first->second = Fix(Make<CStore>(val, ptr));
    }
    return it.first->second;
  }

  /// Returns a load constraint.
  Constraint *Load(Constraint *ptr)
  {
    auto it = dedupLoads_.emplace(ptr, nullptr);
    if (it.second) {
      it.first->second = Make<CLoad>(ptr);
    }
    return it.first->second;
  }

  /// Generates a subset constraint.
  Constraint *Subset(Constraint *a, Constraint *b)
  {
    if (a == b) {
      return nullptr;
    } else {
      auto it = dedupSubset_.emplace(std::make_pair(a, b), nullptr);
      if (it.second) {
        it.first->second = Fix(Make<CSubset>(a, b));
      }
      return it.first->second;
    }
  }

  /// Generates a new, empty set constraint.
  Constraint *Ptr(Bag *bag)
  {
    auto it = dedupPtrs_.emplace(bag, nullptr);
    if (it.second) {
      it.first->second = Make<CPtr>(bag);
    }
    return it.first->second;
  }

  /// Creates an offset constraint, +-inf.
  Constraint *Offset(Constraint *c)
  {
    if (c->Is(Constraint::Kind::OFFSET)) {
      return Offset(static_cast<COffset *>(c)->GetPointer());
    } else {
      auto it = dedupOff_.emplace(std::make_pair(c, std::nullopt), nullptr);
      if (it.second) {
        it.first->second = Make<COffset>(c);
      }
      return it.first->second;
    }
  }

  /// Creates an offset constraint.
  Constraint *Offset(Constraint *c, int64_t offset)
  {
    if (c->Is(Constraint::Kind::OFFSET)) {
      auto *coff = static_cast<COffset *>(c);
      if (auto off = coff->GetOffset()) {
        return Offset(coff->GetPointer(), offset + *off);
      } else {
        return c;
      }
    } else {
      auto it = dedupOff_.emplace(std::make_pair(c, std::optional<int64_t>(offset)), nullptr);
      if (it.second) {
        it.first->second = Make<COffset>(c, offset);
      }
      return it.first->second;
    }
  }

  /// Returns a binary set union.
  Constraint *Union(Constraint *a, Constraint *b)
  {
    if (!a) {
      return b;
    }
    if (!b) {
      return a;
    }

    auto it = dedupUnion_.emplace(std::make_pair(a, b), nullptr);
    if (it.second) {
      it.first->second = Make<CUnion>(a, b);
    }
    return it.first->second;
  }

  /// Returns a ternary set union.
  Constraint *Union(Constraint *a, Constraint *b, Constraint *c)
  {
    return Union(a, Union(b, c));
  }

  /// Indirect call, to be expanded.
  Constraint *Call(
      const std::vector<Inst *> &context,
      Constraint *callee,
      std::vector<Constraint *> args)
  {
    return Fix(Make<CCall>(context, callee, args));
  }

  /// Extern function context.
  Constraint *Extern()
  {
    return extern_;
  }

  /// Generates a new, empty set constraint.
  template<typename ...Args>
  Bag *Bag(Args... args)
  {
    return new class Bag(args...);
  }

  /// Constructs a new node.
  template<typename T, typename ...Args>
  T *Node(Args... args)
  {
    T *node = new T(args...);
    //llvm::errs() << "Node: " << node << "\n";
    return node;
  }

  /// Returns the constraints attached to a function.
  FuncSet &Lookup(const std::vector<Inst *> &calls, Func *func);

  /// Dumps a bag item.
  void Dump(const Bag::Item &item);

  /// Dumps a bag to stdout.
  void Dump(class Bag *bag);

  /// Dumps the constraints to stdout.
  void Dump(const Constraint *c);

  /// Simplifies the constraints.
  void Progress()
  {
    // Remove the dangling nodes which were not fixed.
    for (auto *node : dangling_) {
      Delete(node);
    }
    dangling_.clear();

    fixed_.splice(fixed_.end(), batch_, batch_.begin(), batch_.end());
  }

  /// Iteratively solves the constraints.
  void Iterate();

  /// Simplifies the whole batch.
  std::vector<std::pair<std::vector<Inst *>, Func *>> Expand();

private:
  /// Deletes a node.
  void Delete(Constraint *c);

  /// Constructs a node.
  template<typename T, typename ...Args>
  T *Make(Args... args)
  {
    T *node = new T(args...);
    dangling_.insert(node);
    return node;
  }


  /// Fixes a dangling reference.
  Constraint *Fix(Constraint *c)
  {
    auto it = dangling_.find(c);
    if (it == dangling_.end()) {
      return c;
    }

    dangling_.erase(it);

    switch (c->GetKind()) {
      case Constraint::Kind::PTR: {
        break;
      }
      case Constraint::Kind::SUBSET: {
        auto *csubset = static_cast<CSubset *>(c);
        Fix(csubset->GetSubset());
        Fix(csubset->GetSet());
        break;
      }
      case Constraint::Kind::UNION: {
        auto *cunion = static_cast<CUnion *>(c);
        Fix(cunion->GetLHS());
        Fix(cunion->GetRHS());
        break;
      }
      case Constraint::Kind::OFFSET: {
        auto *coffset = static_cast<COffset *>(c);
        Fix(coffset->GetPointer());
        break;
      }
      case Constraint::Kind::LOAD: {
        auto *cload = static_cast<CLoad *>(c);
        Fix(cload->GetPointer());
        break;
      }
      case Constraint::Kind::STORE: {
        auto *cstore = static_cast<CStore *>(c);
        Fix(cstore->GetValue());
        Fix(cstore->GetPointer());
        break;
      }
      case Constraint::Kind::CALL: {
        auto *ccall = static_cast<CCall *>(c);
        Fix(ccall->GetCallee());
        for (unsigned i = 0; i < ccall->GetNumArgs(); ++i) {
          Fix(ccall->GetArg(i));
        }
        break;
      }
    }

    batch_.push_back(c);

    return c;
  }

private:
  /// Allocated PTR object.
  std::unordered_map<class Bag *, Constraint *> dedupPtrs_;
  /// Allocated LOAD object.
  std::unordered_map<Constraint *, Constraint *> dedupLoads_;
  /// Allocated UNION objects.
  std::map<std::pair<Constraint *, Constraint *>, Constraint *> dedupUnion_;
  /// Allocated OFFFSET object.
  std::map<std::pair<Constraint *, std::optional<int64_t>>, Constraint *> dedupOff_;
  /// Allocated SUBSET object.
  std::map<std::pair<Constraint *, Constraint *>, Constraint *> dedupSubset_;
  /// Allocated STORE object.
  std::map<std::pair<Constraint *, Constraint *>, Constraint *> dedupStore_;

  /// Mapping from nodes to loads.
  std::unordered_map<class Node *, std::set<CLoad *>> loads_;
  /// Function argument/return constraints.
  std::map<Func *, std::unique_ptr<FuncSet>> funcs_;
  /// List of fixed nodes.
  llvm::ilist<Constraint> batch_;
  /// New batch of nodes.
  llvm::ilist<Constraint> fixed_;
  /// Set of dangling nodes.
  std::unordered_set<Constraint *> dangling_;
  /// External bag.
  Constraint *extern_;
  /// Expanded callees for each call site.
  std::unordered_map<CCall *, std::set<Func *>> expanded_;
};
