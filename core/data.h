// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include <llvm/ADT/ilist.h>

#include "core/object.h"
#include "core/global.h"
#include "core/symbol_table.h"

class Prog;
class Data;
class Object;



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
  using ObjectListType = llvm::ilist<Object>;

  /// Iterator over the atoms.
  using iterator = ObjectListType::iterator;
  using const_iterator = ObjectListType::const_iterator;
  /// Reverse iterator over the atoms.
  using reverse_iterator = ObjectListType::reverse_iterator;
  using const_reverse_iterator = ObjectListType::const_reverse_iterator;

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
  llvm::StringRef getName() const { return name_; }

  // Checks if the section is empty.
  bool IsEmpty() const { return objects_.empty(); }

  /// Removes an object.
  void remove(iterator it) { objects_.remove(it); }
  /// Erases an object.
  void erase(iterator it) { objects_.erase(it); }
  /// Adds an object to the segment.
  void AddObject(Object *object, Object *before = nullptr);

  // Iterators over objects.
  bool empty() const { return objects_.empty(); }
  size_t size() const { return objects_.size(); }
  iterator begin() { return objects_.begin(); }
  iterator end() { return objects_.end(); }
  const_iterator begin() const { return objects_.begin(); }
  const_iterator end() const { return objects_.end(); }
  reverse_iterator rbegin() { return objects_.rbegin(); }
  reverse_iterator rend() { return objects_.rend(); }
  const_reverse_iterator rbegin() const { return objects_.rbegin(); }
  const_reverse_iterator rend() const { return objects_.rend(); }

private:
  friend struct llvm::ilist_traits<Data>;
  friend struct llvm::ilist_traits<Object>;
  static ObjectListType Data::*getSublistAccess(Object *) {
    return &Data::objects_;
  }

  /// Updates the parent node.
  void setParent(Prog *parent) { parent_ = parent; }

private:
  /// Program context.
  Prog *parent_;
  /// Name of the segment.
  const std::string name_;
  /// List of objects in the program.
  ObjectListType objects_;
};
