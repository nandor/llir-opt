// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator_range.h>

#include "core/constant.h"
#include "core/expr.h"
#include "core/extern.h"

class Data;
class Func;
class Atom;
class Prog;



/**
 * Traits to handle parent links from functions.
 */
template <> struct llvm::ilist_traits<Func> {
private:
  using instr_iterator = simple_ilist<Func>::iterator;

public:
  void addNodeToList(Func *func);

  void removeNodeFromList(Func *func);

  void transferNodesFromList(
      ilist_traits &from,
      instr_iterator first,
      instr_iterator last
  );

  void deleteNode(Func *func);

  Prog *getParent();
};


/**
 * Program storing all data and functions.
 */
class Prog final {
private:
  /// Type of the function list.
  using FuncListType = llvm::ilist<Func>;
  /// Type of the data segment list.
  using DataListType = llvm::ilist<Data>;
  /// Type of the extern lits.
  using ExternListType = llvm::ilist<Extern>;

  /// Iterator over the functions.
  using iterator = FuncListType::iterator;
  using const_iterator = FuncListType::const_iterator;

  /// Iterator over segments.
  using data_iterator = DataListType::iterator;
  using const_data_iterator = DataListType::const_iterator;

  /// Iterator over externs.
  using ext_iterator = ExternListType::iterator;
  using const_ext_iterator = ExternListType::const_iterator;


public:
  /// Creates a new program.
  Prog();
  /// Deletes a program.
  ~Prog();

  /// Returns a global or creates a dummy extern.
  Global *GetGlobal(const std::string_view name);
  /// Creates a symbol for an atom.
  Atom *CreateAtom(const std::string_view name);
  /// Adds a function to the program.
  Func *CreateFunc(const std::string_view name);

  /// Returns an extern.
  Extern *GetExtern(const std::string_view name);

  /// Creates a new symbol offset expression.
  Expr *CreateSymbolOffset(Global *sym, int64_t offset);
  /// Returns an integer value.
  ConstantInt *CreateInt(int64_t v);
  /// Returns a float value.
  ConstantFloat *CreateFloat(double v);
  /// Returns a register value.
  ConstantReg *CreateReg(ConstantReg::Kind v);

  // Fetches a data segment.
  Data *CreateData(const std::string_view name);

  /// Erases a function.
  void erase(iterator it);
  /// Erases an extern.
  void erase(ext_iterator it);

  /// Adds a function.
  void AddFunc(Func *func, Func *before = nullptr);

  // Iterators over functions.
  size_t size() const { return funcs_.size(); }
  iterator begin() { return funcs_.begin(); }
  iterator end() { return funcs_.end(); }
  const_iterator begin() const { return funcs_.begin(); }
  const_iterator end() const { return funcs_.end(); }
  llvm::iterator_range<const_iterator> funcs() const;
  llvm::iterator_range<iterator> funcs();

  // Iterator over external symbols.
  size_t ext_size() const { return externs_.size(); }
  ext_iterator ext_begin() { return externs_.begin(); }
  ext_iterator ext_end() { return externs_.end(); }
  const_ext_iterator ext_begin() const { return externs_.begin(); }
  const_ext_iterator ext_end() const { return externs_.end(); }
  llvm::iterator_range<const_ext_iterator> externs() const;
  llvm::iterator_range<ext_iterator> externs();

  // Iterator over data segments.
  size_t data_size() const { return datas_.size(); }
  data_iterator data_begin() { return datas_.begin(); }
  data_iterator data_end() { return datas_.end(); }
  const_data_iterator data_begin() const { return datas_.begin(); }
  const_data_iterator data_end() const { return datas_.end(); }
  llvm::iterator_range<const_data_iterator> data() const;
  llvm::iterator_range<data_iterator> data();

private:
  friend struct llvm::ilist_traits<Func>;

  /// Chain of functions.
  FuncListType funcs_;
  /// Chain of data segments.
  DataListType datas_;
  /// List of external symbols.
  ExternListType externs_;
  /// Mapping from names to symbols.
  std::unordered_map<std::string_view, Global *> symbols_;
};
