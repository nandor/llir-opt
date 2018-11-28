// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/StringRef.h>

#include "core/value.h"
#include "core/symbol.h"

class Data;



/**
 * Class representing a value in the data section.
 */
class Item final {
public:
  enum class Kind {
    INT8, INT16, INT32, INT64,
    FLOAT64,
    SYMBOL,
    ALIGN,
    SPACE,
    STRING,
  };

  Item(Kind kind, int8_t val) : kind_(kind), int8val_(val) {}
  Item(Kind kind, int16_t val) : kind_(kind), int16val_(val) {}
  Item(Kind kind, int32_t val) : kind_(kind), int32val_(val) {}
  Item(Kind kind, int64_t val) : kind_(kind), int64val_(val) {}
  Item(Kind kind, unsigned val) : kind_(kind), int64val_(val) {}

  Item(Kind kind, Global *global, int64_t offset)
    : kind_(kind)
  {
    userVal_ = new User(1);
    userVal_->Op<0>() = global;
    offsetVal_ = offset;
  }

  Item(Kind kind, std::string *val) : kind_(kind), stringVal_(val) {}

  ~Item();

  /// Returns the item kind.
  Kind GetKind() const { return kind_; }

  // Returns integer values.
  int64_t GetInt8() const  { assert(kind_ == Kind::INT8);  return int8val_;  }
  int64_t GetInt16() const { assert(kind_ == Kind::INT16); return int16val_; }
  int64_t GetInt32() const { assert(kind_ == Kind::INT32); return int32val_; }
  int64_t GetInt64() const { assert(kind_ == Kind::INT64); return int64val_; }

  // Returns the real values.
  int64_t GetFloat64() const { assert(kind_ == Kind::FLOAT64); return int64val_; }

  /// Returns the spacing.
  unsigned GetSpace() const { assert(kind_ == Kind::SPACE); return int64val_; }
  /// Returns the alignment.
  unsigned GetAlign() const { assert(kind_ == Kind::ALIGN); return int64val_; }

  /// Returns the string value.
  llvm::StringRef GetString() const
  {
    assert(kind_ == Kind::STRING);
    return *stringVal_;
  }

  /// Returns the symbol value.
  Global *GetSymbol() const
  {
    assert(kind_ == Kind::SYMBOL);
    return static_cast<Global *>(userVal_->Op<0>().get());
  }

private:
  /// Value kind.
  Kind kind_;
  /// Value storage.
  union {
    int8_t  int8val_;
    int16_t int16val_;
    int32_t int32val_;
    int64_t int64val_;
    double float64val_;
    struct {
      User *userVal_;
      int64_t offsetVal_;
    };
    std::string *stringVal_;
  };
};


/**
 * Data atom, a symbol followed by some data.
 */
class Atom
  : public llvm::ilist_node_with_parent<Atom, Data>
  , public Global
{
public:
  // Iterators over items.
  typedef std::vector<Item *>::iterator iterator;
  typedef std::vector<Item *>::const_iterator const_iterator;

public:
  /// Creates a new atom.
  Atom(const std::string_view name)
    : Global(name, true)
  {
  }

  /// Deletes the atom.
  ~Atom() override;

  /// Adds an item to the atom.
  void AddItem(Item *item) { items_.push_back(item); }

  // Iterators over items.
  iterator begin() { return items_.begin(); }
  iterator end() { return items_.end(); }
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }

private:
  /// List of data items.
  std::vector<Item *> items_;
};
