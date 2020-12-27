// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ilist.h>

#include "core/value.h"
#include "core/global.h"
#include "core/symbol_table.h"
#include "core/item.h"

class Data;
class Expr;




/**
 * A symbol followed by data items.
 */
class Atom
  : public llvm::ilist_node_with_parent<Atom, Object>
  , public Global
{
public:
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::ATOM;

public:
  /// Type of the item list.
  using ItemListType = llvm::ilist<Item>;
  // Iterators over items.
  typedef ItemListType::iterator iterator;
  typedef ItemListType::const_iterator const_iterator;

public:
  /// Creates a new parent.
  Atom(
      const std::string_view name,
      Visibility visibility = Visibility::LOCAL,
      std::optional<llvm::Align> align = std::nullopt)
    : Global(Global::Kind::ATOM, name, visibility, 0)
    , parent_(nullptr)
    , align_(align)
  {
  }

  /// Deletes the parent.
  ~Atom() override;

  /// Removes an atom from the data section.
  void removeFromParent() override;
  /// Removes an parent from the data section, erasing it.
  void eraseFromParent() override;

  /// Returns a pointer to the parent section.
  Object *getParent() const { return parent_; }

  /// Removes an atom.
  void remove(iterator it) { items_.remove(it); }
  /// Erases an atom.
  void erase(iterator it) { items_.erase(it); }
  /// Adds an atom to the atom.
  void AddItem(Item *atom, Item *before = nullptr);

  // Iterators over items.
  bool empty() const { return items_.empty(); }
  size_t size() const { return items_.size(); }
  iterator begin() { return items_.begin(); }
  iterator end() { return items_.end(); }
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }
  /// Clears all items.
  void clear() { items_.clear(); }

  /// Returns the size of the atom in bytes.
  size_t GetByteSize() const;
  /// Changes the parent alignment.
  void SetAlignment(llvm::Align align) { align_ = align; }
  /// Returns the parent alignment.
  std::optional<llvm::Align> GetAlignment() const override { return align_; }

  /// Returns the program to which the atom belongs.
  Prog *getProg() override;

private:
  friend struct SymbolTableListTraits<Atom>;
  friend struct llvm::ilist_traits<Item>;
  static ItemListType Atom::*getSublistAccess(Item *) {
    return &Atom::items_;
  }

  /// Updates the parent node.
  void setParent(Object *parent) { parent_ = parent; }

private:
  /// Object the atom is part of.
  Object *parent_;
  /// List of data items.
  ItemListType items_;
  /// Alignment of the parent.
  std::optional<llvm::Align> align_;
};
