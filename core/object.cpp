// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/object.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/printer.h"



// -----------------------------------------------------------------------------
Object::~Object()
{
}

// -----------------------------------------------------------------------------
void Object::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Object::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Object::AddAtom(Atom *atom, Atom *before)
{
  if (before == nullptr) {
    atoms_.push_back(atom);
  } else {
    atoms_.insert(before->getIterator(), atom);
  }
}

// -----------------------------------------------------------------------------
void Object::dump(llvm::raw_ostream &os) const
{
  Printer(os).Print(*this);
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::deleteNode(Object *object)
{
  delete object;
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::addNodeToList(Object *object)
{
  assert(!object->getParent() && "node already in list");
  Data *data = getParent();
  object->setParent(data);
  if (Prog *parent = data->getParent()) {
    for (Atom &atom : *object) {
      parent->insertGlobal(&atom);
    }
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::removeNodeFromList(Object *object)
{
  Data *data = getParent();
  object->setParent(nullptr);

  if (Prog *parent = data->getParent()) {
    for (Atom &atom : *object) {
      parent->removeGlobalName(atom.GetName());
    }
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
  Data *data = getParent();
  for (auto it = first; it != last; ++it) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
Data *llvm::ilist_traits<Object>::getParent()
{
  auto sublist = Data::getSublistAccess(static_cast<Object *>(nullptr));
  size_t offset(size_t(&(static_cast<Data *>(nullptr)->*sublist)));
  Data::ObjectListType *anchor(static_cast<Data::ObjectListType *>(this));
  return reinterpret_cast<Data *>(reinterpret_cast<char*>(anchor) - offset);
}
