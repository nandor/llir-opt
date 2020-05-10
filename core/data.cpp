// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/prog.h"

#include <llvm/Support/raw_ostream.h>

// -----------------------------------------------------------------------------
Data::~Data()
{
}

// -----------------------------------------------------------------------------
void Data::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Data::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Data::AddAtom(Atom *atom, Atom *before)
{
  if (before == nullptr) {
    atoms_.push_back(atom);
  } else {
    atoms_.insert(before->getIterator(), atom);
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Data>::deleteNode(Data *data)
{
  delete data;
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Data>::addNodeToList(Data *data)
{
  assert(!data->getParent() && "node already in list");
  Prog *parent = getParent();
  data->setParent(parent);
  for (Atom &atom : *data) {
    parent->insertGlobal(&atom);
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Data>::removeNodeFromList(Data *data)
{
  Prog *parent = getParent();
  data->setParent(nullptr);
  for (Atom &atom : *data) {
    parent->removeGlobalName(atom.GetName());
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Data>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
  Prog *parent = getParent();
  for (auto it = first; it != last; ++it) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
Prog *llvm::ilist_traits<Data>::getParent()
{
  auto sublist = Prog::getSublistAccess(static_cast<Data *>(nullptr));
  size_t offset(size_t(&(static_cast<Prog *>(nullptr)->*sublist)));
  Prog::DataListType *anchor(static_cast<Prog::DataListType *>(this));
  return reinterpret_cast<Prog *>(reinterpret_cast<char*>(anchor) - offset);
}
