// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include <llvm/ADT/ilist.h>

#include "core/atom.h"
#include "core/global.h"
#include "core/symbol_table.h"

class Prog;
class Data;



/**
 * Traits to handle parent links from data segments.
 */
template <> struct llvm::ilist_traits<Data> {
private:
  using instr_iterator = simple_ilist<Data>::iterator;

public:
  void deleteNode(Data *inst);
  void addNodeToList(Data *inst);
  void removeNodeFromList(Data *inst);
  void transferNodesFromList(
      ilist_traits &from,
      instr_iterator first,
      instr_iterator last
  );

  Prog *getParent();
};


/**
 * The data segment of a program.
 */
class Data final : public llvm::ilist_node_with_parent<Data, Prog> {
private:
  /// Type of the function list.
  using AtomListType = SymbolTableList<Atom>;

  /// Iterator over the functions.
  using iterator = AtomListType::iterator;
  using const_iterator = AtomListType::const_iterator;

public:
  // Initialises the data segment.
  Data(const std::string_view name)
    : parent_(nullptr)
    , name_(name)
  {
  }

  /// Deletes the data segment.
  ~Data();

  /// Removes the segment from the parent.
  void removeFromParent();
  /// Removes an parent from the data section.
  void eraseFromParent();

  /// Returns a pointer to the parent section.
  Prog *getParent() const { return parent_; }

  // Returns the segment name.
  std::string_view GetName() const { return name_; }
  /// Returns the segment name.
  llvm::StringRef getName() const { return name_.c_str(); }

  // Checks if the section is empty.
  bool IsEmpty() const { return atoms_.empty(); }

  /// Removes an atom.
  void remove(iterator it) { atoms_.remove(it); }
  /// Erases an atom.
  void erase(iterator it) { atoms_.erase(it); }
  /// Adds an atom to the segment.
  void AddAtom(Atom *atom, Atom *before = nullptr);

  // Iterators over atoms.
  bool empty() const { return atoms_.empty(); }
  size_t size() const { return atoms_.size(); }
  iterator begin() { return atoms_.begin(); }
  iterator end() { return atoms_.end(); }
  const_iterator begin() const { return atoms_.begin(); }
  const_iterator end() const { return atoms_.end(); }

private:
  friend struct llvm::ilist_traits<Data>;
  friend class SymbolTableListTraits<Atom>;
  static AtomListType Data::*getSublistAccess(Atom *) {
    return &Data::atoms_;
  }

  /// Updates the parent node.
  void setParent(Prog *parent) { parent_ = parent; }

private:
  /// Program context.
  Prog *parent_;
  /// Name of the segment.
  const std::string name_;
  /// List of atoms in the program.
  AtomListType atoms_;
};
