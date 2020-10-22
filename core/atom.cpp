// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/atom.h"
#include "core/data.h"
#include "core/expr.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Item::Item(const std::string_view str)
  : kind_(Kind::STRING)
  , parent_(nullptr)
  , stringVal_(new std::string(str))
{
}

// -----------------------------------------------------------------------------
Item::~Item()
{
  switch (kind_) {
    case Item::Kind::INT8:
    case Item::Kind::INT16:
    case Item::Kind::INT32:
    case Item::Kind::INT64:
    case Item::Kind::FLOAT64:
    case Item::Kind::ALIGN:
    case Item::Kind::SPACE: {
      return;
    }
    case Item::Kind::EXPR: {
      delete exprVal_;
      return;
    }
    case Item::Kind::STRING: {
      delete stringVal_;
      return;
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
void Item::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
Atom::~Atom()
{
}

// -----------------------------------------------------------------------------
void Atom::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Atom::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
Prog *Atom::getProg()
{
  return getParent()->getParent()->getParent();
}

// -----------------------------------------------------------------------------
void Atom::AddItem(Item *item, Item *before)
{
  if (before == nullptr) {
    items_.push_back(item);
  } else {
    items_.insert(before->getIterator(), item);
  }
}


// -----------------------------------------------------------------------------
void llvm::ilist_traits<Item>::deleteNode(Item *item)
{
  delete item;
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Item>::addNodeToList(Item *item)
{
  assert(!item->getParent() && "node already in list");
  item->setParent(getParent());
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Item>::removeNodeFromList(Item *item)
{
  item->setParent(nullptr);
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Item>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
  Atom *parent = getParent();
  for (auto it = first; it != last; ++it) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
Atom *llvm::ilist_traits<Item>::getParent()
{
  auto sublist = Atom::getSublistAccess(static_cast<Item *>(nullptr));
  size_t offset(size_t(&(static_cast<Atom *>(nullptr)->*sublist)));
  Atom::ItemListType *anchor(static_cast<Atom::ItemListType *>(this));
  return reinterpret_cast<Atom *>(reinterpret_cast<char*>(anchor) - offset);
}
