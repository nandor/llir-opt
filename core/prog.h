// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator_range.h>


class Data;
class Func;



/**
 * Program storing all data and functions.
 */
class Prog {
private:
  /// Type of the function list.
  using FuncListType = llvm::ilist<Func>;

  /// Iterator over the functions.
  using iterator = FuncListType::iterator;
  using const_iterator = FuncListType::const_iterator;

  /// Iterator over externs.
  using ext_iterator = std::vector<Global *>::iterator;
  using const_ext_iterator = std::vector<Global *>::const_iterator;

public:
  /// Creates a new program.
  Prog();

  /// Creates a symbol for a function.
  Global *CreateSymbol(const std::string_view name);
  /// Adds a function to the program.
  Func *AddFunc(const std::string_view name);
  /// Adds an external symbol.
  void AddExternal(const std::string_view name);

  // Fetch data segments.
  Data *GetData() const { return data_; }
  Data *GetBSS() const { return bss_; }
  Data *GetConst() const { return const_; }

  // Iterators over functions.
  iterator begin() { return funcs_.begin(); }
  iterator end() { return funcs_.end(); }
  const_iterator begin() const { return funcs_.begin(); }
  const_iterator end() const { return funcs_.end(); }

  // Iterator over external symbols.
  ext_iterator ext_begin() { return externs_.begin(); }
  ext_iterator ext_end() { return externs_.end(); }
  const_ext_iterator ext_begin() const { return externs_.begin(); }
  const_ext_iterator ext_end() const { return externs_.end(); }
  llvm::iterator_range<const_ext_iterator> externs() const;
  llvm::iterator_range<ext_iterator> externs();

private:
  /// .data segment
  Data *data_;
  /// .bss segment
  Data *bss_;
  /// .const segment
  Data *const_;
  /// Chain of functions.
  FuncListType funcs_;
  /// Mapping from names to symbols.
  std::unordered_map<std::string_view, Global *> symbols_;
  /// List of external symbols.
  std::vector<Global *> externs_;
};
