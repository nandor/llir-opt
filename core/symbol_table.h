// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/simple_ilist.h>

class Func;
class Prog;
class Block;
class Atom;
class Extern;
class Data;
class Global;
template <typename T> class SymbolTableList;



// Parent types for all global symbols.
template <typename T> struct SymbolTableListParentType {};
template <> struct SymbolTableListParentType<Func> { using type = Prog; };
template <> struct SymbolTableListParentType<Atom> { using type = Data; };
template <> struct SymbolTableListParentType<Extern> { using type = Prog; };
template <> struct SymbolTableListParentType<Block> { using type = Func; };

/**
 * Traits for maintaining nodes in a symbol table.
 */
template <typename T>
class SymbolTableListTraits : public llvm::ilist_alloc_traits<T> {
private:
  using ListTy = SymbolTableList<T>;
  using iterator = typename llvm::simple_ilist<T>::iterator;
  using ParentTy = typename SymbolTableListParentType<T>::type;

public:
  ParentTy *getParent();

  void addNodeToList(T *V);
  void removeNodeFromList(T *V);
  void transferNodesFromList(
      SymbolTableListTraits &L2,
      iterator first,
      iterator last
  );

  Prog *getProg(ParentTy *parent);
};


/**
 * Functions are special symbol table entries since they track blocks.
 */
template <>
class SymbolTableListTraits<Func> : public llvm::ilist_alloc_traits<Func>  {
private:
  using iterator = typename llvm::simple_ilist<Func>::iterator;

public:
  Prog *getParent();

  void addNodeToList(Func *V);
  void removeNodeFromList(Func *V);
  void transferNodesFromList(
      SymbolTableListTraits &L2,
      iterator first,
      iterator last
  );
};


/**
 * List of symbol table entries.
 */
template <class T>
class SymbolTableList
  : public llvm::iplist_impl<llvm::simple_ilist<T>, SymbolTableListTraits<T>>
{
};
