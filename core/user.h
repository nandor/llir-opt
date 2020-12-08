// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <iterator>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>

#include "core/value.h"



/**
 * Value which references other values.
 */
class User : public Value {
public:
  using op_iterator = Use *;
  using const_op_iterator = const Use *;
  using op_range = llvm::iterator_range<op_iterator>;
  using const_op_range = llvm::iterator_range<const_op_iterator>;

  struct value_op_iterator : llvm::iterator_adaptor_base
      < value_op_iterator
      , op_iterator
      , std::random_access_iterator_tag
      , Ref<Value>
      , ptrdiff_t
      , Ref<Value>
      , Ref<Value>>
  {
    explicit value_op_iterator(Use *U = nullptr)
      : iterator_adaptor_base(U)
    {
    }

    Ref<Value> operator*() const { return *I; }
    Ref<Value> operator->() const { return *I; }
  };

  struct const_value_op_iterator : llvm::iterator_adaptor_base
      < const_value_op_iterator
      , const_op_iterator
      , std::random_access_iterator_tag
      , ConstRef<Value>
      , ptrdiff_t
      , ConstRef<Value>
      , ConstRef<Value>>
  {
    explicit const_value_op_iterator(const Use *U = nullptr)
      : iterator_adaptor_base(U)
    {
    }

    ConstRef<Value> operator*() const { return *I; }
    ConstRef<Value> operator->() const { return *I; }
  };

  using value_op_range = llvm::iterator_range<value_op_iterator>;
  using const_value_op_range = llvm::iterator_range<const_value_op_iterator>;

  template <typename T>
  struct conv_op_iterator : llvm::iterator_adaptor_base
      < conv_op_iterator<T>
      , value_op_iterator
      , std::random_access_iterator_tag
      , Ref<T>
      , ptrdiff_t
      , Ref<T>
      , Ref<T>
      >
  {
    conv_op_iterator(User::value_op_iterator it)
      : llvm::iterator_adaptor_base
          < conv_op_iterator<T>
          , value_op_iterator
          , std::random_access_iterator_tag
          , Ref<T>
          , ptrdiff_t
          , Ref<T>
          , Ref<T>
          >(it)
    {
    }

    Ref<T> operator*() const;
    Ref<T> operator->() const;
  };

  template <typename T>
  struct const_conv_op_iterator : llvm::iterator_adaptor_base
      < const_conv_op_iterator<T>
      , const_value_op_iterator
      , std::random_access_iterator_tag
      , ConstRef<T>
      , ptrdiff_t
      , ConstRef<T>
      , ConstRef<T>
      >
  {
    const_conv_op_iterator(User::const_value_op_iterator it)
      : llvm::iterator_adaptor_base
          < const_conv_op_iterator<T>
          , const_value_op_iterator
          , std::random_access_iterator_tag
          , ConstRef<T>
          , ptrdiff_t
          , ConstRef<T>
          , ConstRef<T>
          >(it)
    {
    }

    ConstRef<T> operator*() const;
    ConstRef<T> operator->() const;
  };

  template<typename T>
  using conv_op_range = llvm::iterator_range<conv_op_iterator<T>>;

  template<typename T>
  using const_conv_op_range = llvm::iterator_range<const_conv_op_iterator<T>>;

public:
  /// Creates a new user.
  User(Kind kind, unsigned numOps);

  /// Cleans up after the use.
  virtual ~User();

  // Returns the number of operands.
  size_t size() const { return numOps_; }

  // Iterators over uses.
  op_iterator op_begin() { return uses_; }
  op_iterator op_end() { return uses_ + numOps_; }
  op_range operands();
  const_op_iterator op_begin() const { return uses_; }
  const_op_iterator op_end() const { return uses_ + numOps_; }
  const_op_range operands() const;

  // Iterators over values.
  value_op_iterator value_op_begin();
  value_op_iterator value_op_end();
  value_op_range operand_values();
  const_value_op_iterator value_op_begin() const;
  const_value_op_iterator value_op_end() const;
  const_value_op_range operand_values() const;

protected:
  /// Setter for an operand.
  template <int I>
  void Set(Ref<Value> val)
  {
    (I < 0 ? op_end() : op_begin())[I] = val;
  }

  /// Setter for an operand.
  void Set(int i, Ref<Value> val)
  {
    (i < 0 ? op_end() : op_begin())[i] = val;
  }

  /// Accessor for an operand.
  template <int I>
  Ref<Value> Get()
  {
    return static_cast<Ref<Value> &>((I < 0 ? op_end() : op_begin())[I]);
  }

  /// Accessor for an operand.
  Ref<Value> Get(int i)
  {
    return static_cast<Ref<Value> &>((i < 0 ? op_end() : op_begin())[i]);
  }

  /// Accessor for an operand.
  template <int I>
  ConstRef<Value> Get() const
  {
    return static_cast<ConstRef<Value>>((I < 0 ? op_end() : op_begin())[I]);
  }

  /// Accessor for an operand.
  ConstRef<Value> Get(int i) const
  {
    return static_cast<ConstRef<Value>>((i < 0 ? op_end() : op_begin())[i]);
  }

protected:
  /// Grows the operand list.
  void resizeUses(unsigned n);

protected:
  /// Number of operands.
  unsigned numOps_;
  /// Head of the use list.
  Use *uses_;
};
