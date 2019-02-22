// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/raw_ostream.h>

#include "core/block.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts_call.h"
#include "passes/global_data_elim/constraint.h"
#include "passes/global_data_elim/solver.h"



// -----------------------------------------------------------------------------
static bool IsSet(Constraint *c, Constraint *user)
{
  if (user->Is(Constraint::Kind::SUBSET)) {
    auto *csubset = static_cast<CSubset *>(user);
    if (csubset->GetSet() == c) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Iterate()
{
  llvm::errs() << "Iterate: " << fixed_.size() << "\n";

  std::vector<std::tuple<Constraint *, Constraint *, Bag::Item>> queue;
  for (auto &node : fixed_) {
    if (node.Is(Constraint::Kind::PTR)) {
      auto &cptr = static_cast<CPtr &>(node);
      cptr.GetBag()->ForEach([&cptr, &queue](auto &item) {
        for (auto *user : cptr.users()) {
          if (!IsSet(&cptr, user)) {
            queue.emplace_back(&cptr, user, item);
          }
        }
      });
      continue;
    }
    if (node.Is(Constraint::Kind::LOAD)) {
      auto &cload = static_cast<CLoad &>(node);
      cload.GetPtrSet()->ForEach([&cload, &queue](auto &item) {
        queue.emplace_back(nullptr, &cload, item);
      });
      continue;
    }
    if (node.Is(Constraint::Kind::CALL)) {
      auto &ccall = static_cast<CCall &>(node);
      ccall.GetRetSet()->ForEach([&ccall, &queue](auto &item) {
        for (auto *user : ccall.users()) {
          if (!IsSet(&ccall, user)) {
            queue.emplace_back(&ccall, user, item);
          }
        }
      });
      continue;
    }
  }

  while (!queue.empty()) {
    auto [from, to, item] = queue.back();
    queue.pop_back();

    switch (to->GetKind()) {
      case Constraint::Kind::PTR: {
        assert(!"not implemented");
        break;
      }
      case Constraint::Kind::SUBSET: {
        // Subset node - add to resulting set.
        auto *csubset = static_cast<CSubset *>(to);
        
        auto *set = csubset->GetSet();
        switch (set->GetKind()) {
          case Constraint::Kind::PTR: {
            auto *cptr = static_cast<CPtr *>(set);
            if (cptr->GetBag()->Store(item)) {
              for (auto *user : cptr->users()) {
                if (!IsSet(cptr, user)) {
                  queue.emplace_back(cptr, user, item);
                }
              }
            }
            break;
          }
          case Constraint::Kind::CALL: {
            auto *ccall = static_cast<CCall *>(set);
            if (ccall->GetRetSet()->Store(item)) {
              for (auto *user : ccall->users()) {
                if (!IsSet(ccall, user)) {
                  queue.emplace_back(ccall, user, item);
                }
              }
            }
            break;
          }
          default: {
            assert(!"not implemented");
          }
        }
        break;
      }
      case Constraint::Kind::UNION: {
        // Union node - simple pass through.
        for (auto *user : to->users()) {
          if (!IsSet(to, user)) {
            queue.emplace_back(to, user, item);
          }
        }
        break;
      }
      case Constraint::Kind::OFFSET: {
        // Offset node - propagate item with offset to users.
        auto *coffset = static_cast<COffset *>(to);
        if (auto newItem = item.Offset(coffset->GetOffset())) {
          for (auto *user : to->users()) {
            if (!IsSet(to, user)) {
              queue.emplace_back(to, user, *newItem);
            }
          }
        }
        break;
      }
      case Constraint::Kind::LOAD: {
        // Load node - propagate values loaded from the node.
        auto *cload = static_cast<CLoad *>(to);

        auto *ptrSet = cload->GetPtrSet();
        auto *valSet = cload->GetValSet();

        if (cload->GetPointer() == from) {
          // A new pointer was propagated to load from.
          if (ptrSet->Store(item)) {
            if (auto node = item.GetNode()) {
              loads_[node->first].insert(cload);
            }

            item.Load([valSet, cload, &queue](auto &newItem) {
              if (valSet->Store(newItem)) {
                for (auto *user : cload->users()) {
                  if (!IsSet(cload, user)) {
                    queue.emplace_back(cload, user, newItem);
                  }
                }
              }
            });
          }
        } else {
          // A new value was propagated from a store.
          // It is actually the pointer - load again.
          item.Load([valSet, cload, &queue](auto &newItem) {
            if (valSet->Store(newItem)) {
              for (auto *user : cload->users()) {
                if (!IsSet(cload, user)) {
                  queue.emplace_back(cload, user, newItem);
                }
              }
            }
          });
        }
        break;
      }
      case Constraint::Kind::STORE: {
        // Store node - propagate values to loads.
        auto *cstore = static_cast<CStore *>(to);

        auto *ptrSet = cstore->GetPtrSet();
        bool newPtr = false;
        if (from == cstore->GetPointer()) {
          newPtr = ptrSet->Store(item);
        }

        auto *valSet = cstore->GetValSet();
        bool newVal = false;
        if (from == cstore->GetValue()) {
          newVal = valSet->Store(item);
        }

        if (newPtr) {
          valSet->ForEach([itemPtr = item, cstore, &queue, this](auto &itemVal) {
            if (itemPtr.Store(itemVal)) {
              if (auto node = itemPtr.GetNode()) {
                for (auto *load : loads_[node->first]) {
                  if (load->GetPtrSet()->Contains(itemPtr)) {
                    queue.emplace_back(cstore, load, itemPtr);
                  }
                }
              }
            }
          });
        }

        if (newVal) {
          ptrSet->ForEach([itemVal = item, cstore, &queue, this](auto &itemPtr) {
            if (itemPtr.Store(itemVal)) {
              if (auto node = itemPtr.GetNode()) {
                for (auto *load : loads_[node->first]) {
                  if (load->GetPtrSet()->Contains(itemPtr)) {
                    queue.emplace_back(cstore, load, itemPtr);
                  }
                }
              }
            }
          });
        }

        break;
      }
      case Constraint::Kind::CALL: {
        // Call node - expanded later, potential callees are collected here.
        auto *ccall = static_cast<CCall *>(to);
        if (ccall->GetCallee() == from) {
          ccall->GetPtrSet()->Store(item);
        }
        break;
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> ConstraintSolver::Expand()
{
  Iterate();

  std::vector<std::pair<std::vector<Inst *>, Func *>> callees;
  for (auto &node : fixed_) {
    if (!node.Is(Constraint::Kind::CALL)) {
      continue;
    }

    auto &call = static_cast<CCall &>(node);
    call.GetPtrSet()->ForEach([&callees, &call, this](auto &item) {
      if (auto *func = item.GetFunc()) {
        // Only expand each call site once.
        auto &expanded = expanded_[&call];
        if (!expanded.insert(func).second) {
          return;
        }

        // Deduplicate the list of returned callees.
        callees.emplace_back(call.GetContext(), func);

        // Connect arguments and return value.
        auto &funcSet = this->Lookup(call.GetContext(), func);
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
void ConstraintSolver::Delete(Constraint *c)
{
  switch (c->GetKind()) {
    case Constraint::Kind::PTR: {
      auto *cptr = static_cast<CPtr *>(c);
      dedupPtrs_.erase(cptr->GetBag());
      break;
    }
    case Constraint::Kind::SUBSET: {
      auto *csubset = static_cast<CSubset *>(c);
      dedupSubset_.erase(std::make_pair(csubset->GetSubset(), csubset->GetSet()));
      break;
    }
    case Constraint::Kind::UNION: {
      auto *cunion = static_cast<CUnion *>(c);
      dedupUnion_.erase(std::make_pair(cunion->GetLHS(), cunion->GetRHS()));
      break;
    }
    case Constraint::Kind::OFFSET: {
      auto *coffset = static_cast<COffset *>(c);
      dedupOff_.erase(std::make_pair(coffset->GetPointer(), coffset->GetOffset()));
      break;
    }
    case Constraint::Kind::LOAD: {
      auto *cload = static_cast<CLoad *>(c);
      dedupLoads_.erase(cload->GetPointer());
      break;
    }
    case Constraint::Kind::STORE: {
      auto *cstore = static_cast<CStore *>(c);
      dedupStore_.erase(std::make_pair(cstore->GetValue(), cstore->GetPointer()));
      break;
    }
    case Constraint::Kind::CALL: {
      break;
    }
  }
  delete c;
}

// -----------------------------------------------------------------------------
ConstraintSolver::FuncSet &ConstraintSolver::Lookup(
    const std::vector<Inst *> &calls, 
    Func *func)
{
  auto key = func;
  auto it = funcs_.emplace(key, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<FuncSet>();
    auto f = it.first->second.get();
    f->Return = Fix(Ptr(Bag()));
    f->VA = Fix(Ptr(Bag()));
    f->Frame = Fix(Ptr(Bag()));
    for (auto &arg : func->params()) {
      f->Args.push_back(Fix(Ptr(Bag())));
    }
    f->Expanded = false;
  }
  return *it.first->second;
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
