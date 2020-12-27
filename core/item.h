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
  enum class Kind : uint8_t {
    INT8, INT16, INT32, INT64,
    FLOAT64,
    EXPR,
    SPACE,
    STRING
  };

  struct Space { unsigned V; };

  Item(int8_t val) : kind_(Kind::INT8), parent_(nullptr), int8val_(val) {}
  Item(int16_t val) : kind_(Kind::INT16), parent_(nullptr), int16val_(val) {}
  Item(int32_t val) : kind_(Kind::INT32), parent_(nullptr), int32val_(val) {}
  Item(int64_t val) : kind_(Kind::INT64), parent_(nullptr), int64val_(val) {}
  Item(double val) : kind_(Kind::FLOAT64), parent_(nullptr), float64val_(val) {}
  Item(Space val) : kind_(Kind::SPACE), parent_(nullptr), int32val_(val.V) {}
  Item(Expr *val);
  Item(const std::string_view str);

  ~Item();

  /// Removes an atom from the data section.
  void eraseFromParent();

  /// Returns a pointer to the parent section.
  Atom *getParent() const { return parent_; }

  /// Returns the item kind.
  Kind GetKind() const { return kind_; }

  /// Returns the size of the item in bytes.
  size_t GetSize() const;

  // Returns integer values.
  int8_t GetInt8() const  { assert(kind_ == Kind::INT8);  return int8val_;  }
  int16_t GetInt16() const { assert(kind_ == Kind::INT16); return int16val_; }
  int32_t GetInt32() const { assert(kind_ == Kind::INT32); return int32val_; }
  int64_t GetInt64() const { assert(kind_ == Kind::INT64); return int64val_; }
  /// Returns the spacing.
  unsigned GetSpace() const { assert(kind_ == Kind::SPACE); return int32val_; }

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
