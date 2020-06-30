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

  Item(Atom *parent, int8_t val) : kind_(Kind::INT8), parent_(parent), int8val_(val) {}
  Item(Atom *parent, int16_t val) : kind_(Kind::INT16), parent_(parent), int16val_(val) {}
  Item(Atom *parent, int32_t val) : kind_(Kind::INT32), parent_(parent), int32val_(val) {}
  Item(Atom *parent, int64_t val) : kind_(Kind::INT64), parent_(parent), int64val_(val) {}
  Item(Atom *parent, double val) : kind_(Kind::FLOAT64), parent_(parent), float64val_(val) {}
  Item(Atom *parent, Expr *val) : kind_(Kind::EXPR), parent_(parent), exprVal_(val) {}
  Item(Atom *parent, Align val) : kind_(Kind::ALIGN), parent_(parent), int32val_(val.V) {}
  Item(Atom *parent, Space val) : kind_(Kind::SPACE), parent_(parent), int32val_(val.V) {}
  Item(Atom *parent, const std::string_view str);

  ~Item();

  /// Removes an atom from the data section.
  void eraseFromParent();

  /// Returns a pointer to the parent section.
  Atom *getParent() const { return parent_; }

  /// Returns the item kind.
  Kind GetKind() const { return kind_; }

  // Returns integer values.
  int64_t GetInt8() const  { assert(kind_ == Kind::INT8);  return int8val_;  }
  int64_t GetInt16() const { assert(kind_ == Kind::INT16); return int16val_; }
  int64_t GetInt32() const { assert(kind_ == Kind::INT32); return int32val_; }
  int64_t GetInt64() const { assert(kind_ == Kind::INT64); return int64val_; }
  /// Returns the spacing.
  unsigned GetSpace() const { assert(kind_ == Kind::SPACE); return int32val_; }
  /// Returns the alignment.
  unsigned GetAlign() const { assert(kind_ == Kind::ALIGN); return int32val_; }

  // Returns the real values.
  int64_t GetFloat64() const
  {
    assert(kind_ == Kind::FLOAT64);
    return int64val_;
  }

  /// Returns the string value.
  llvm::StringRef GetString() const
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
      unsigned align = 1)
    : Global(Global::Kind::ATOM, name, 0, visibility)
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

  /// Adds an item to the parent.
  void AddAlignment(unsigned i);
  void AddSpace(unsigned i);
  void AddString(const std::string_view str);
  void AddInt8(int8_t v);
  void AddInt16(int16_t v);
  void AddInt32(int32_t v);
  void AddInt64(int64_t v);
  void AddFloat64(int64_t v);
  void AddFloat64(double v);
  void AddExpr(Expr *expr);
  void AddSymbol(Global *global, int64_t offset);

  /// Erases an item.
  void erase(iterator it);

  // Iterators over items.
  size_t size() const { return items_.size(); }
  iterator begin() { return items_.begin(); }
  iterator end() { return items_.end(); }
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }

  /// Changes the parent alignment.
  void SetAlignment(unsigned align) { align_ = align; }
  /// Returns the parent alignment.
  unsigned GetAlignment() const override { return align_; }

private:
  friend struct SymbolTableListTraits<Atom>;
  /// Updates the parent node.
  void setParent(Object *parent) { parent_ = parent; }

private:
  /// Object the atom is part of.
  Object *parent_;
  /// List of data items.
  ItemListType items_;
  /// Alignment of the parent.
  unsigned align_;
};
