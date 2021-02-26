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
#include "core/object.h"
#include "core/func.h"
#include "core/xtor.h"

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
  /// Type of the extern list.
  using ExternListType = SymbolTableList<Extern>;
  /// Type of the constructor/destructor list.
  using XtorListType = llvm::ilist<Xtor>;

  /// Iterator over all globals.
  using GlobalMap = std::unordered_map<std::string_view, Global *>;

  class global_iterator
    : public llvm::iterator_adaptor_base
        < global_iterator
        , GlobalMap::iterator
        , std::random_access_iterator_tag
        , Global *
        >
  {
  public:
    explicit global_iterator(GlobalMap::iterator it)
      : iterator_adaptor_base(it)
    {
    }

    Global *operator*() const { return this->I->second; }

    Global *operator->() const { return this->I->second; }
  };

  class const_global_iterator
    : public llvm::iterator_adaptor_base
        < const_global_iterator
        , GlobalMap::const_iterator
        , std::random_access_iterator_tag
        , const Global *
        >
  {
  public:
    explicit const_global_iterator(GlobalMap::const_iterator it)
      : iterator_adaptor_base(it)
    {
    }

    const Global *operator*() const { return this->I->second; }

    const Global *operator->() const { return this->I->second; }
  };

public:
  /// Iterator over the functions.
  using iterator = FuncListType::iterator;
  using const_iterator = FuncListType::const_iterator;

  /// Iterator over segments.
  using data_iterator = DataListType::iterator;
  using const_data_iterator = DataListType::const_iterator;

  /// Iterator over externs.
  using ext_iterator = ExternListType::iterator;
  using const_ext_iterator = ExternListType::const_iterator;

  /// Iterator over ctors and dtors.
  using xtor_iterator = XtorListType::iterator;
  using const_xtor_iterator = XtorListType::const_iterator;

public:
  /// Creates a new program.
  Prog(std::string_view path);
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
  Global *GetGlobal(const std::string_view name) const;

  /// Returns the name of the program.
  const std::string &GetName() const { return name_; }
  /// Returns the name of the program.
  llvm::StringRef getName() const { return { name_.data(), name_.size() }; }

  /// Removes a function.
  void remove(iterator it) { funcs_.remove(it); }
  /// Erases a function.
  void erase(iterator it) { funcs_.erase(it); }
  /// Removes an extern.
  void remove(ext_iterator it) { externs_.remove(it); }
  /// Erases an extern.
  void erase(ext_iterator it) { externs_.erase(it); }
  /// Removes a data segment.
  void remove(data_iterator it) { datas_.remove(it); }
  /// Erases a data segment.
  void erase(data_iterator it) { datas_.erase(it); }
  /// Removes a constructor/destructor.
  void remove(xtor_iterator it) { xtors_.remove(it); }
  /// Erases a constructor/destructor.
  void erase(xtor_iterator it) { xtors_.erase(it); }

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

  /// Add a constructor or a destructor.
  void AddXtor(Xtor *xtor, Xtor *before = nullptr);
  // Iterator over constructors/destructors.
  size_t xtor_size() const { return xtors_.size(); }
  bool xtor_empty() const { return xtors_.empty(); }
  xtor_iterator xtor_begin() { return xtors_.begin(); }
  xtor_iterator xtor_end() { return xtors_.end(); }
  const_xtor_iterator xtor_begin() const { return xtors_.begin(); }
  const_xtor_iterator xtor_end() const { return xtors_.end(); }
  llvm::iterator_range<const_xtor_iterator> xtor() const;
  llvm::iterator_range<xtor_iterator> xtor();

  /// Range of globals.
  global_iterator global_begin() { return global_iterator(globals_.begin()); }
  global_iterator global_end() { return global_iterator(globals_.end()); }
  const_global_iterator global_begin() const { return const_global_iterator(globals_.begin()); }
  const_global_iterator global_end() const { return const_global_iterator(globals_.end()); }
  llvm::iterator_range<global_iterator> globals();
  llvm::iterator_range<const_global_iterator> globals() const;

  /// Dumps the representation of the function.
  void dump(llvm::raw_ostream &os = llvm::errs()) const;

private:
  /// Accessors for the symbol table.
  friend class SymbolTableListTraits<Block>;
  friend class SymbolTableListTraits<Func>;
  friend class SymbolTableListTraits<Extern>;
  friend class SymbolTableListTraits<Atom>;
  friend struct llvm::ilist_traits<Data>;
  friend struct llvm::ilist_traits<Object>;

  void insertGlobal(Global *g);
  void removeGlobalName(std::string_view name);

  static FuncListType Prog::*getSublistAccess(Func *) { return &Prog::funcs_; }
  static ExternListType Prog::*getSublistAccess(Extern *) { return &Prog::externs_; }
  static DataListType Prog::*getSublistAccess(Data *) { return &Prog::datas_; }
  static XtorListType Prog::*getSublistAccess(Xtor *) { return &Prog::xtors_; }

private:
  /// Name of the program.
  std::string name_;
  /// Mapping from names to symbols.
  std::unordered_map<std::string_view, Global *> globals_;
  /// Chain of functions.
  FuncListType funcs_;
  /// Chain of data segments.
  DataListType datas_;
  /// List of external symbols.
  ExternListType externs_;
  /// List of constructors and destructors.
  XtorListType xtors_;
};
