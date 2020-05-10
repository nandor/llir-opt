// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <map>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator_range.h>

#include "core/constant.h"
#include "core/expr.h"
#include "core/extern.h"
#include "core/symbol_table.h"
#include "core/data.h"

class Func;
class Block;
class Prog;
class Extern;



/**
 * Program storing all data and functions.
 */
class Prog final {
private:
  /// Type of the function list.
  using FuncListType = SymbolTableList<Func>;
  /// Type of the data segment list.
  using DataListType = llvm::ilist<Data>;
  /// Type of the extern lits.
  using ExternListType = SymbolTableList<Extern>;

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
  Global *GetGlobalOrExtern(const std::string_view name);
  /// Returns an extern.
  Extern *GetExtern(const std::string_view name);
  /// Fetches a data segment, creates it if it does not exist.
  Data *GetOrCreateData(const std::string_view name);
  /// Fetches a data segment.
  Data *GetData(const std::string_view name);
  /// Fetches a global.
  Global *GetGlobal(const std::string_view name);

  /// Removes a function.
  void remove(iterator it);
  /// Erases a function.
  void erase(iterator it);
  /// Removes an extern.
  void remove(ext_iterator it);
  /// Erases an extern.
  void erase(ext_iterator it);
  /// Removes a data segment.
  void remove(data_iterator it);
  /// Erases a data segment.
  void erase(data_iterator it);

  /// Adds a function.
  void AddFunc(Func *func, Func *before = nullptr);
  // Iterators over functions.
  size_t size() const { return funcs_.size(); }
  bool empty() const { return funcs_.empty(); }
  iterator begin() { return funcs_.begin(); }
  iterator end() { return funcs_.end(); }
  const_iterator begin() const { return funcs_.begin(); }
  const_iterator end() const { return funcs_.end(); }
  llvm::iterator_range<const_iterator> funcs() const;
  llvm::iterator_range<iterator> funcs();

  /// Adds an extern.
  void AddExtern(Extern *ext, Extern *before = nullptr);
  // Iterator over external symbols.
  size_t ext_size() const { return externs_.size(); }
  ext_iterator ext_begin() { return externs_.begin(); }
  ext_iterator ext_end() { return externs_.end(); }
  const_ext_iterator ext_begin() const { return externs_.begin(); }
  const_ext_iterator ext_end() const { return externs_.end(); }
  llvm::iterator_range<const_ext_iterator> externs() const;
  llvm::iterator_range<ext_iterator> externs();

  /// Adds a data item.
  void AddData(Data *data, Data *before = nullptr);
  // Iterator over data segments.
  size_t data_size() const { return datas_.size(); }
  bool data_empty() const { return datas_.empty(); }
  data_iterator data_begin() { return datas_.begin(); }
  data_iterator data_end() { return datas_.end(); }
  const_data_iterator data_begin() const { return datas_.begin(); }
  const_data_iterator data_end() const { return datas_.end(); }
  llvm::iterator_range<const_data_iterator> data() const;
  llvm::iterator_range<data_iterator> data();

private:
  /// Accessors for the symbol table.
  friend class SymbolTableListTraits<Block>;
  friend class SymbolTableListTraits<Func>;
  friend class SymbolTableListTraits<Extern>;
  friend class SymbolTableListTraits<Atom>;
  friend struct llvm::ilist_traits<Data>;
  friend class Data;

  void insertGlobal(Global *g);
  void removeGlobalName(std::string_view name);

  static FuncListType Prog::*getSublistAccess(Func *) { return &Prog::funcs_; }
  static ExternListType Prog::*getSublistAccess(Extern *) { return &Prog::externs_; }
  static DataListType Prog::*getSublistAccess(Data *) { return &Prog::datas_; }

private:
  /// Mapping from names to symbols.
  std::unordered_map<std::string_view, Global *> globals_;
  /// Chain of functions.
  FuncListType funcs_;
  /// Chain of data segments.
  DataListType datas_;
  /// List of external symbols.
  ExternListType externs_;
  /// Mapping from names to symbols.
  std::map<std::string, Global *, std::less<>> symbols_;
};
