// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/StringRef.h>

#include "core/use.h"

class Item;
class Expr;
class Atom;



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
  /// Enumeration of item kinds.
  enum class Kind : uint8_t {
    /// 8-bit integer.
    INT8,
    /// 16-bit integer.
    INT16,
    /// 32-bit integer.
    INT32,
    /// 64-bit integer.
    INT64,
    /// IEEE double.
    FLOAT64,
    /// 32-bit pointer.
    EXPR32,
    /// 64-bit pointer.
    EXPR64,
    /// Unallocated space.
    SPACE,
    /// Raw string.
    STRING
  };

public:
  /// Copy constructor.
  Item(Item &that);
  /// Cleanup.
  ~Item();

  // Helpers to create items.
  static Item *CreateInt8(int8_t val);
  static Item *CreateInt16(int16_t val);
  static Item *CreateInt32(int32_t val);
  static Item *CreateInt64(int64_t val);
  static Item *CreateFloat64(double val);
  static Item *CreateSpace(unsigned val);
  static Item *CreateExpr32(Expr *val);
  static Item *CreateExpr64(Expr *val);
  static Item *CreateString(const std::string_view str);

  /// Removes an item from the parent.
  void removeFromParent();
  /// Removes an atom from the data section.
  void eraseFromParent();

  /// Returns a pointer to the parent section.
  Atom *getParent() const { return parent_; }

  /// Returns the item kind.
  Kind GetKind() const { return kind_; }

  /// Checks whether the item is an expression.
  bool IsExpr() const
  {
    return kind_ == Kind::EXPR32 || kind_ == Kind::EXPR64;
  }

  /// Checks whether the item is space.
  bool IsSpace() const { return GetKind() == Item::Kind::SPACE; }

  /// Returns the size of the item in bytes.
  size_t GetSize() const;

  // Returns integer values.
  int8_t GetInt8() const  { assert(kind_ == Kind::INT8);  return int8val_;  }
  int16_t GetInt16() const { assert(kind_ == Kind::INT16); return int16val_; }
  int32_t GetInt32() const { assert(kind_ == Kind::INT32); return int32val_; }
  int64_t GetInt64() const { assert(kind_ == Kind::INT64); return int64val_; }
  /// Returns the spacing.
  unsigned GetSpace() const { assert(IsSpace()); return int32val_; }

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
    return stringVal_;
  }

  /// Returns the string value.
  std::string_view GetString() const
  {
    assert(kind_ == Kind::STRING);
    return stringVal_;
  }

  /// Returns the symbol value.
  Expr *GetExpr();
  /// Returns the symbol value.
  const Expr *GetExpr() const;
  /// Returns the item as an expression, nullptr if not one.
  Expr *AsExpr();
  /// Returns the item as an expression, nullptr if not one.
  const Expr *AsExpr() const;

private:
  /// Create a new item of a specific kind.
  Item(Kind kind) : kind_(kind), parent_(nullptr) {}

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
    Use           useVal_;
    std::string   stringVal_;
  };
};
