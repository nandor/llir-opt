// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include <llvm/ADT/ilist.h>

#include "core/symbol.h"
#include "core/atom.h"

class Prog;



/**
 * The data segment of a program.
 */
class Data {
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

  // Methods to populate atoms.
  void Align(unsigned i);
  void AddSpace(unsigned i);
  void AddString(const std::string &str);
  void AddInt8(Const *v);
  void AddInt16(Const *v);
  void AddInt32(Const *v);
  void AddInt64(Const *v);
  void AddFloat64(Const *v);

  /// Adds a symbol and an atom to the segment.
  Atom *CreateAtom(const std::string_view name);

  // Iterators over atoms.
  iterator begin() { return atoms_.begin(); }
  iterator end() { return atoms_.end(); }
  const_iterator begin() const { return atoms_.begin(); }
  const_iterator end() const { return atoms_.end(); }

private:
  /// Returns the current atom.
  Atom *GetAtom();

private:
  /// Program context.
  Prog *prog_;
  /// Name of the segment.
  const std::string name_;
  /// List of atoms in the program.
  AtomListType atoms_;
};
