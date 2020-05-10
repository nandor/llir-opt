// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/symbol_table.h"
#include "core/func.h"
#include "core/atom.h"
#include "core/extern.h"
#include "core/block.h"
#include "core/prog.h"
#include "core/data.h"



// -----------------------------------------------------------------------------
template <typename T>
typename SymbolTableListTraits<T>::ParentTy *
SymbolTableListTraits<T>::getParent()
{
  auto sublist = ParentTy::getSublistAccess(static_cast<T*>(nullptr));
  size_t offset(size_t(&(static_cast<ParentTy *>(nullptr)->*sublist)));
  ListTy *anchor(static_cast<ListTy *>(this));
  return reinterpret_cast<ParentTy *>(reinterpret_cast<char*>(anchor) - offset);
}

// -----------------------------------------------------------------------------
template <typename T>
Prog *SymbolTableListTraits<T>::getProg(ParentTy *parent)
{
  if (!parent) {
    return nullptr;
  }

  if constexpr (std::is_same<ParentTy, Prog>::value) {
    return parent;
  } else {
    if (auto *prog = parent->getParent()) {
      return prog;
    }
    return nullptr;
  }
}

// -----------------------------------------------------------------------------
template <typename T>
void SymbolTableListTraits<T>::addNodeToList(T *node)
{
  assert(!node->getParent() && "Value already in a container!");
  ParentTy *parent = getParent();
  node->setParent(parent);
  if (auto *table = getProg(parent)) {
    table->insertGlobal(node);
  }
}

// -----------------------------------------------------------------------------
template <typename T>
void SymbolTableListTraits<T>::removeNodeFromList(T *node) {
  node->setParent(nullptr);
  if (auto *table = getProg(getParent())) {
    table->removeGlobalName(node->GetName());
  }
}

// -----------------------------------------------------------------------------
template <typename T>
void SymbolTableListTraits<T>::transferNodesFromList(
    SymbolTableListTraits &L2,
    iterator first,
    iterator last)
{
  ParentTy *newParent = getParent(), *oldParent = L2.getParent();
  assert(newParent != oldParent && "Expected different list owners");

  Prog *newProg = getProg(newParent);
  Prog *oldProg = getProg(oldParent);
  if (newProg != oldProg) {
    for (auto it = first; it != last; ++it) {
      T &V = *it;
      if (oldProg) {
        oldProg->removeGlobalName(V.GetName());
      }
      V.setParent(newParent);
      if (newProg) {
        newProg->insertGlobal(&V);
      }
    }
  } else {
    for (auto it = first; it != last; ++it) {
      it->setParent(newParent);
    }
  }
}

// -----------------------------------------------------------------------------
template class SymbolTableListTraits<Func>;
template class SymbolTableListTraits<Block>;
template class SymbolTableListTraits<Atom>;
template class SymbolTableListTraits<Extern>;
