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
Prog *getProg(T *child)
{
  if (!child) {
    return nullptr;
  }

  if constexpr (std::is_same<T, Prog>::value) {
    return child;
  } else {
    return getProg(child->getParent());
  }
}

// -----------------------------------------------------------------------------
template <typename T>
void SymbolTableListTraits<T>::addNodeToList(T *node)
{
  assert(!node->getParent() && "Value already in a container!");
  ParentTy *parent = getParent();
  node->setParent(parent);
  if (auto *table = getProg<ParentTy>(parent)) {
    table->insertGlobal(node);
  }
}

// -----------------------------------------------------------------------------
template <typename T>
void SymbolTableListTraits<T>::removeNodeFromList(T *node) {
  node->setParent(nullptr);
  if (auto *table = getProg<ParentTy>(getParent())) {
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

  Prog *newProg = getProg<ParentTy>(newParent);
  Prog *oldProg = getProg<ParentTy>(oldParent);
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
Prog *SymbolTableListTraits<Func>::getParent()
{
  auto sublist = Prog::getSublistAccess(static_cast<Func*>(nullptr));
  size_t offset(size_t(&(static_cast<Prog *>(nullptr)->*sublist)));
  Prog::FuncListType *anchor(static_cast<Prog::FuncListType *>(this));
  return reinterpret_cast<Prog *>(reinterpret_cast<char*>(anchor) - offset);
}

// -----------------------------------------------------------------------------
void SymbolTableListTraits<Func>::addNodeToList(Func *func)
{
  assert(!func->getParent() && "func already in list");
  Prog *parent = getParent();
  func->setParent(parent);
  parent->insertGlobal(func);
  for (Block &block : *func) {
    parent->insertGlobal(&block);
  }
}

// -----------------------------------------------------------------------------
void SymbolTableListTraits<Func>::removeNodeFromList(Func *func)
{
  Prog *parent = getParent();
  func->setParent(nullptr);
  for (Block &block : *func) {
    parent->removeGlobalName(block.GetName());
  }
  if (auto *prog = func->getParent()) {
    prog->removeGlobalName(func->GetName());
  }
}

// -----------------------------------------------------------------------------
void SymbolTableListTraits<Func>::transferNodesFromList(
    SymbolTableListTraits &L2,
    iterator first,
    iterator last)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
template class SymbolTableListTraits<Block>;
template class SymbolTableListTraits<Atom>;
template class SymbolTableListTraits<Extern>;
