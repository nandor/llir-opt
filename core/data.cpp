// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Data::Data(const std::string_view name)
  : parent_(nullptr)
  , name_(name)
{
}

// -----------------------------------------------------------------------------
Data::~Data()
{
}

// -----------------------------------------------------------------------------
bool Data::IsZeroed() const
{
  return llvm::StringRef(name_).startswith(".bss");
}

// -----------------------------------------------------------------------------
bool Data::IsWritable() const
{
  llvm::StringRef name(name_);
  return !name.startswith(".bss") && !name.startswith(".interp");
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
void Data::AddObject(Object *object, Object *before)
{
  if (before == nullptr) {
    objects_.push_back(object);
  } else {
    objects_.insert(before->getIterator(), object);
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
  for (Object &object : *data) {
    for (Atom &atom : object) {
      parent->insertGlobal(&atom);
    }
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Data>::removeNodeFromList(Data *data)
{
  Prog *parent = getParent();
  data->setParent(nullptr);
  for (Object &object : *data) {
    for (Atom &atom : object) {
      parent->removeGlobalName(atom.GetName());
    }
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
