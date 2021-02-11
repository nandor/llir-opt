// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>
#include <map>
#include <unordered_set>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/cast.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/object_split.h"



// -----------------------------------------------------------------------------
const char *ObjectSplitPass::kPassID = "object-split";

// -----------------------------------------------------------------------------
const char *ObjectSplitPass::GetPassName() const
{
  return "Object Splitting";
}


// -----------------------------------------------------------------------------
using RefSet = std::unordered_set<Ref<Inst>>;

// -----------------------------------------------------------------------------
static std::optional<std::map<std::pair<int64_t, Type>, RefSet>>
FindUses(Atom &atom)
{
  std::queue<std::pair<User *, int64_t>> qu;
  std::queue<std::pair<Inst *, std::pair<Ref<Inst>, int64_t>>> qi;
  for (User *user : atom.users()) {
    qu.emplace(user, 0);
  }

  while (!qu.empty()) {
    auto [u, off] = qu.front();
    qu.pop();
    if (!u) {
      return std::nullopt;
    }

    switch (u->GetKind()) {
      case Value::Kind::INST: {
        auto *inst = static_cast<Inst *>(u);
        qi.emplace(inst, std::make_pair(nullptr, off));
        continue;
      }
      case Value::Kind::EXPR: {
        switch (static_cast<Expr *>(u)->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto *expr = static_cast<SymbolOffsetExpr *>(u);
            for (User *user : expr->users()) {
              qu.emplace(user, off + expr->GetOffset());
            }
            continue;
          }
        }
        llvm_unreachable("not implemented");
      }
      case Value::Kind::GLOBAL: {
        switch (static_cast<Global *>(u)->GetKind()) {
          case Global::Kind::FUNC:
          case Global::Kind::BLOCK:
          case Global::Kind::ATOM: {
            llvm_unreachable("invalid item user");
          }
          case Global::Kind::EXTERN: {
            return std::nullopt;
          }
        }
        llvm_unreachable("invalid global kind");
      }
      case Value::Kind::CONST: {
        llvm_unreachable("invalid user");
      }
    }
    llvm_unreachable("invalid value kind");
  }

  std::map<std::pair<int64_t, Type>, RefSet> access;
  while (!qi.empty()) {
    auto [i, refAndOffset] = qi.front();
    auto [ref, off] = refAndOffset;
    qi.pop();

    switch (i->GetKind()) {
      default: return std::nullopt;
      case Inst::Kind::LOAD: {
        auto &load = static_cast<LoadInst &>(*i);
        auto key = std::make_pair(off, load.GetType());
        access[key].insert(load.GetAddr());
        continue;
      }
      case Inst::Kind::STORE: {
        auto &store = static_cast<StoreInst &>(*i);
        auto value = store.GetValue();
        if (value == ref) {
          return std::nullopt;
        }
        auto key = std::make_pair(off, value.GetType());
        access[key].insert(store.GetAddr());
        continue;
      }
      case Inst::Kind::MOV: {
        for (User *user : i->users()) {
          if (auto *inst = ::cast_or_null<Inst>(user)) {
            qi.emplace(inst, std::make_pair(i, off));
          }
        }
        continue;
      }
    }
  }
  return std::move(access);
}

// -----------------------------------------------------------------------------
bool ObjectSplitPass::Run(Prog &prog)
{
  bool changed = false;
  for (Data &data : prog.data()) {
    llvm::SmallVector<Object *, 4> newObjects;
    for (Object &obj : data) {
      if (obj.size() != 1) {
        continue;
      }
      Atom &atom = *obj.begin();
      if (!atom.IsLocal()) {
        continue;
      }
      auto uses = FindUses(atom);
      if (!uses || uses->size() <= 1) {
        continue;
      }
      // Ensure the object is access through non-aliased/non-overlapping uses.
      bool overlaps = false;
      for (auto it = uses->begin(); it != uses->end(); ++it) {
        auto is = it->first.first;
        auto ie = is + GetSize(it->first.second);
        for (auto jt = std::next(it); jt != uses->end(); ++jt) {
          auto js = jt->first.first;
          auto je = js + GetSize(jt->first.second);
          if (ie <= js || je <= is) {
            continue;
          }
          overlaps = true;
          break;
        }
        if (overlaps) {
          break;
        }
      }
      if (overlaps) {
        continue;
      }

      // Split the object.
      auto it = atom.begin();
      unsigned startOff = 0;
      unsigned itOff = 0;
      for (auto [location, insts] : *uses) {
        auto [off, ty] = location;

        // Advance the iterator.
        while (off > startOff) {
          auto itSize = it->GetSize();
          if (startOff + itSize <= off) {
            startOff += itSize;
            itOff = 0;
            ++it;
          } else {
            llvm_unreachable("not implemented");
          }
        }

        // Create a new atom.
        std::string name;
        llvm::raw_string_ostream os(name);
        os << atom.getName() << "$" << off << "$" << ty;

        auto *newObject = new Object();
        newObjects.push_back(newObject);
        auto *newAtom = new Atom(name, Visibility::LOCAL, GetAlignment(ty));
        newObject->AddAtom(newAtom);

        // Replace insts with the atom.
        for (auto inst : insts) {
          auto *newMov = new MovInst(Type::I64, newAtom, {});
          inst->getParent()->AddInst(newMov, &*inst);
          inst->replaceAllUsesWith(newMov);
          inst->eraseFromParent();
        }

        // Populate the atom with the required item.
        unsigned size = GetSize(ty);
        while (size > 0) {
          auto itSize = it->GetSize();
          if (itOff + size >= itSize) {
            if (itOff == 0) {
              newAtom->AddItem(new Item(*it));
              startOff += itSize;
              size -= itSize;
              ++it;
            } else {
              unsigned left = itSize - itOff;
              switch (it->GetKind()) {
                case Item::Kind::INT8: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::INT16: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::INT32: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::INT64: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::FLOAT64: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::SPACE: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::EXPR: {
                  llvm_unreachable("not implemented");
                }
                case Item::Kind::STRING: {
                  auto chunk = it->GetString().substr(itOff, left);
                  newAtom->AddItem(new Item(chunk));
                  startOff += left;
                  size -= left;
                  itOff = 0;
                  ++it;
                  continue;
                }
              }
              llvm_unreachable("unknown item kind");
            }
          } else {
            switch (it->GetKind()) {
              case Item::Kind::INT8: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::INT16: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::INT32: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::INT64: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::FLOAT64: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::SPACE: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::EXPR: {
                llvm_unreachable("not implemented");
              }
              case Item::Kind::STRING: {
                auto chunk = it->GetString().substr(itOff, size);
                newAtom->AddItem(new Item(chunk));
                startOff += size;
                itOff += size;
                size = 0;
                continue;
              }
            }
            llvm_unreachable("unknown item kind");
          }
        }
      }
    }
    for (Object *object : newObjects) {
      data.AddObject(object);
      changed = true;
    }
  }
  return changed;
}
