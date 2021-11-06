// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ilist.h>

#include "core/atom.h"
#include "core/symbol_table.h"

class Object;
class Prog;
class Atom;



/**
 * Traits to handle parent links from data segments.
 */
template <> struct llvm::ilist_traits<Object> {
private:
  using instr_iterator = simple_ilist<Object>::iterator;

public:
  void deleteNode(Object *inst);
  void addNodeToList(Object *inst);
  void removeNodeFromList(Object *inst);
  void transferNodesFromList(
      ilist_traits &from,
      instr_iterator first,
      instr_iterator last
  );

  Data *getParent();
};



/**
 * The data segment of a program.
 */
class Object final : public llvm::ilist_node_with_parent<Object, Prog> {
private:
  /// Type of the function list.
  using AtomListType = SymbolTableList<Atom>;

  /// Iterator over the atoms.
  using iterator = AtomListType::iterator;
  using const_iterator = AtomListType::const_iterator;
  /// Reverse iterator over the atoms.
  using reverse_iterator = AtomListType::reverse_iterator;
  using const_reverse_iterator = AtomListType::const_reverse_iterator;

public:
  // Initialises the data segment.
  Object() : parent_(nullptr), isThreadLocal_(false) {}

  /// Deletes the data segment.
  ~Object();

  /// Removes the segment from the parent.
  void removeFromParent();
  /// Removes an parent from the data section.
  void eraseFromParent();

  /// Returns a pointer to the parent section.
  Data *getParent() const { return parent_; }

  /// Removes an atom.
  void remove(iterator it) { atoms_.remove(it); }
  /// Erases an atom.
  void erase(iterator it) { atoms_.erase(it); }
  /// Adds an atom to the atom.
  void AddAtom(Atom *atom, Atom *before = nullptr);

  // Iterators over atoms.
  bool empty() const { return atoms_.empty(); }
  size_t size() const { return atoms_.size(); }
  iterator begin() { return atoms_.begin(); }
  iterator end() { return atoms_.end(); }
  const_iterator begin() const { return atoms_.begin(); }
  const_iterator end() const { return atoms_.end(); }
  reverse_iterator rbegin() { return atoms_.rbegin(); }
  reverse_iterator rend() { return atoms_.rend(); }
  const_reverse_iterator rbegin() const { return atoms_.rbegin(); }
  const_reverse_iterator rend() const { return atoms_.rend(); }

  /// Dump the object.
  void dump(llvm::raw_ostream &os = llvm::errs()) const;

  /// Load a value from an offset.
  Value *Load(uint64_t offset, Type type);
  /// Store a value to an offset.
  bool Store(uint64_t offset, Ref<Value> value, Type type);

  /// Set the thread local flag.
  void SetThreadLocal(bool flag = true) { isThreadLocal_ = flag; }
  /// Check whether the object is thread-local.
  bool IsThreadLocal() const { return isThreadLocal_; }

private:
  friend struct llvm::ilist_traits<Object>;
  friend class SymbolTableListTraits<Atom>;
  static AtomListType Object::*getSublistAccess(Atom *) {
    return &Object::atoms_;
  }

  /// Updates the parent node.
  void setParent(Data *parent) { parent_ = parent; }

private:
  /// Parent segment.
  Data *parent_;
  /// List of atoms in the object.
  AtomListType atoms_;
  /// Flag to indicate whether the object is thread local.
  bool isThreadLocal_;
};

/// Print the value to a stream.
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Object &obj)
{
  obj.dump(os);
  return os;
}
