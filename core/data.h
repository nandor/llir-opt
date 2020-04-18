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

class Prog;



/**
 * The data segment of a program.
 */
class Data final : public llvm::ilist_node<Data> {
private:
  /// Type of the function list.
  using AtomListType = llvm::ilist<Atom>;

  /// Iterator over the functions.
  using iterator = AtomListType::iterator;
  using const_iterator = AtomListType::const_iterator;

public:
  // Initialises the data segment.
  Data(Prog *prog, const std::string_view name)
    : prog_(prog)
    , name_(name)
  {
  }

  // Returns the segment name.
  std::string_view GetName() const { return name_; }
  /// Returns the segment name.
  llvm::StringRef getName() const { return name_.c_str(); }

  // Checks if the section is empty.
  bool IsEmpty() const { return atoms_.empty(); }

  /// Adds a symbol and an atom to the segment.
  Atom *CreateAtom(const std::string_view name);

  /// Erases an atom.
  void erase(iterator it);

  // Iterators over atoms.
  size_t size() const { return atoms_.size(); }
  iterator begin() { return atoms_.begin(); }
  iterator end() { return atoms_.end(); }
  const_iterator begin() const { return atoms_.begin(); }
  const_iterator end() const { return atoms_.end(); }

private:
  /// Program context.
  Prog *prog_;
  /// Name of the segment.
  const std::string name_;
  /// List of atoms in the program.
  AtomListType atoms_;
};
