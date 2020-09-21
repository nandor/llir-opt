// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/StringRef.h>

#include "core/value.h"
#include "core/global.h"
#include "core/symbol_table.h"

class Atom;
class Data;
class Expr;
class Item;



/**
 * Traits to handle parent links from items.
 */
template <> struct llvm::ilist_traits<Item> {
private:
  using instr_iterator = simple_ilist<Item>::iterator;

public:
  void deleteNode(Item *inst);
  void addNodeToList(Item *inst);
  void removeNodeFromList(Item *inst);
  void transferNodesFromList(
      ilist_traits &from,
      instr_iterator first,
      instr_iterator last
  );

  Atom *getParent();
};

/**
 * Class representing a value in the data section.
 */
class Item final : public llvm::ilist_node_with_parent<Item, Atom> {
public:
  enum class Kind : uint8_t {
    INT8, INT16, INT32, INT64,
    FLOAT64,
    EXPR,
    ALIGN,
    SPACE,
    STRING
  };

  struct Align { unsigned V; };
  struct Space { unsigned V; };

  Item(int8_t val) : kind_(Kind::INT8), parent_(nullptr), int8val_(val) {}
  Item(int16_t val) : kind_(Kind::INT16), parent_(nullptr), int16val_(val) {}
  Item(int32_t val) : kind_(Kind::INT32), parent_(nullptr), int32val_(val) {}
  Item(int64_t val) : kind_(Kind::INT64), parent_(nullptr), int64val_(val) {}
  Item(double val) : kind_(Kind::FLOAT64), parent_(nullptr), float64val_(val) {}
  Item(Expr *val) : kind_(Kind::EXPR), parent_(nullptr), exprVal_(val) {}
  Item(Align val) : kind_(Kind::ALIGN), parent_(nullptr), int32val_(val.V) {}
  Item(Space val) : kind_(Kind::SPACE), parent_(nullptr), int32val_(val.V) {}
  Item(const std::string_view str);

  ~Item();

  /// Removes an atom from the data section.
  void eraseFromParent();

  /// Returns a pointer to the parent section.
  Atom *getParent() const { return parent_; }

  /// Returns the item kind.
  Kind GetKind() const { return kind_; }

  // Returns integer values.
  int8_t GetInt8() const  { assert(kind_ == Kind::INT8);  return int8val_;  }
  int16_t GetInt16() const { assert(kind_ == Kind::INT16); return int16val_; }
  int32_t GetInt32() const { assert(kind_ == Kind::INT32); return int32val_; }
  int64_t GetInt64() const { assert(kind_ == Kind::INT64); return int64val_; }
  /// Returns the spacing.
  unsigned GetSpace() const { assert(kind_ == Kind::SPACE); return int32val_; }
  /// Returns the alignment.
  unsigned GetAlign() const { assert(kind_ == Kind::ALIGN); return int32val_; }

  // Returns the real values.
  double GetFloat64() const
  {
    assert(kind_ == Kind::FLOAT64);
    return float64val_;
  }

  /// Returns the string value.
  llvm::StringRef getString() const
  {
    assert(kind_ == Kind::STRING);
    return *stringVal_;
  }

  /// Returns the string value.
  std::string_view GetString() const
  {
    assert(kind_ == Kind::STRING);
    return *stringVal_;
  }

  /// Returns the symbol value.
  Expr *GetExpr() const
  {
    assert(kind_ == Kind::EXPR);
    return exprVal_;
  }

  /// Returns the item as an expression, nullptr if not one.
  Expr *AsExpr() const
  {
    return kind_ == Kind::EXPR ? exprVal_ : nullptr;
  }

private:
  friend struct llvm::ilist_traits<Item>;

  void setParent(Atom *parent) { parent_ = parent; }

private:
  /// Value kind.
  Kind kind_;
  /// Atom of which this item is part of.
  Atom *parent_;
  /// Value storage.
  union {
    int8_t        int8val_;
    int16_t       int16val_;
    int32_t       int32val_;
    int64_t       int64val_;
    double        float64val_;
    Expr *        exprVal_;
    std::string * stringVal_;
  };
};


/**
 * A symbol followed by data items.
 */
class Atom
  : public llvm::ilist_node_with_parent<Atom, Object>
  , public Global
{
public:
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::ATOM;

public:
  /// Type of the item list.
  using ItemListType = llvm::ilist<Item>;
  // Iterators over items.
  typedef ItemListType::iterator iterator;
  typedef ItemListType::const_iterator const_iterator;

public:
  /// Creates a new parent.
  Atom(
      const std::string_view name,
      Visibility visibility = Visibility::HIDDEN,
      bool exported = false,
      llvm::Align align = llvm::Align(1))
    : Global(Global::Kind::ATOM, name, visibility, exported, 0)
    , parent_(nullptr)
    , align_(align)
  {
  }

  /// Deletes the parent.
  ~Atom() override;

  /// Removes an atom from the data section.
  void removeFromParent() override;
  /// Removes an parent from the data section, erasing it.
  void eraseFromParent() override;

  /// Returns a pointer to the parent section.
  Object *getParent() const { return parent_; }

  /// Removes an atom.
  void remove(iterator it) { items_.remove(it); }
  /// Erases an atom.
  void erase(iterator it) { items_.erase(it); }
  /// Adds an atom to the atom.
  void AddItem(Item *atom, Item *before = nullptr);

  // Iterators over items.
  bool empty() const { return items_.empty(); }
  size_t size() const { return items_.size(); }
  iterator begin() { return items_.begin(); }
  iterator end() { return items_.end(); }
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }
  /// Clears all items.
  void clear() { items_.clear(); }

  /// Changes the parent alignment.
  void SetAlignment(llvm::Align align) { align_ = align; }
  /// Returns the parent alignment.
  llvm::Align GetAlignment() const override { return align_; }

private:
  friend struct SymbolTableListTraits<Atom>;
  friend struct llvm::ilist_traits<Item>;
  static ItemListType Atom::*getSublistAccess(Item *) {
    return &Atom::items_;
  }

  /// Updates the parent node.
  void setParent(Object *parent) { parent_ = parent; }

private:
  /// Object the atom is part of.
  Object *parent_;
  /// List of data items.
  ItemListType items_;
  /// Alignment of the parent.
  llvm::Align align_;
};
