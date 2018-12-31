// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <iterator>

#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>

class Use;
class User;
class Value;


/**
 * Use site of a value.
 */
class Use final {
public:
  /// Creates a new use in an object for a value.
  Use(Value *val, User *user)
    : val_(val)
    , user_(user)
    , prev_(nullptr)
    , next_(nullptr)
  {
  }

  /// Destroys a use.
  ~Use();

  /// Do not allow copy constructors.
  Use(const Use &) = delete;
  /// Do not allow move constructors.
  Use(Use &&) = delete;

  /// Assign a new value.
  Use &operator = (Value *val);

  /// Do not allow assignment.
  Use &operator = (const Use &) = delete;
  /// Do not allow move assignment.
  Use &operator = (Use &&) = delete;

  /// Returns the user attached to this use.
  User *getUser() const { return user_; }
  /// Returns the next use.
  Use *getNext() const { return next_; }

  // Return the underlying value.
  operator Value *() const { return val_; }
  Value *get() const { return val_; }

  // Point to the underlying value.
  Value *operator->() { return val_; }
  const Value *operator->() const { return val_; }

private:
  friend class Value;
  friend class User;

  void Remove();
  void Add();

  /// Used value.
  Value *val_;
  /// Pointer to the user.
  User *user_;
  /// Previous use.
  Use *prev_;
  /// Next use.
  Use *next_;
};


/**
 * Base class of all values.
 */
class Value {
public:
  template<typename T>
  using forward_it = std::iterator<std::forward_iterator_tag, T>;

  /**
   * Iterator over the uses.
   */
  template <typename UseT>
  class use_iterator_impl : public forward_it<UseT *> {
  public:
    use_iterator_impl() : U() {}

    bool operator==(const use_iterator_impl &x) const { return U == x.U; }
    bool operator!=(const use_iterator_impl &x) const { return !operator==(x); }

    use_iterator_impl &operator++() {
      assert(U && "Cannot increment end iterator!");
      U = U->getNext();
      return *this;
    }

    use_iterator_impl operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    UseT &operator*() const {
      assert(U && "Cannot dereference end iterator!");
      return *U;
    }

    UseT *operator->() const { return &operator*(); }

    operator use_iterator_impl<const UseT>() const {
      return use_iterator_impl<const UseT>(U);
    }

  private:
    explicit use_iterator_impl(UseT *u) : U(u) {}

  private:
    friend class Value;
    UseT *U;
  };

  /**
   * Iterator over the users.
   */
  template <typename UserTy>
  class user_iterator_impl : public forward_it<UserTy *> {
  public:
    user_iterator_impl() = default;

    bool operator==(const user_iterator_impl &x) const { return UI == x.UI; }
    bool operator!=(const user_iterator_impl &x) const { return !operator==(x); }

    /// Returns true if this iterator is equal to user_end() on the value.
    bool atEnd() const { return *this == user_iterator_impl(); }

    user_iterator_impl &operator++() { // Preincrement
      ++UI;
      return *this;
    }

    user_iterator_impl operator++(int) { // Postincrement
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    // Retrieve a pointer to the current User.
    UserTy *operator*() const {
      return UI->getUser();
    }

    UserTy *operator->() const { return operator*(); }

    operator user_iterator_impl<const UserTy>() const {
      return user_iterator_impl<const UserTy>(*UI);
    }

    Use &getUse() const { return *UI; }

  private:
    explicit user_iterator_impl(Use *U) : UI(U) {}

  private:
    use_iterator_impl<Use> UI;
    friend class Value;
  };

  using use_iterator = use_iterator_impl<Use>;
  using const_use_iterator = use_iterator_impl<const Use>;
  using user_iterator = user_iterator_impl<User>;
  using const_user_iterator = user_iterator_impl<const User>;

public:
  /// Enumeration of value types.
  enum class Kind {
    INST,
    GLOBAL,
    EXPR,
    CONST,
  };

  /// Constructs a new value.
  Value(Kind kind) : kind_(kind), users_(nullptr) { }
  /// Do not allow copying.
  Value(const Value &) = delete;
  /// Do not allow moving.
  Value(Value &&) = delete;

  /// Destroy the value.
  virtual ~Value();

  /// Returns the value kind.
  Kind GetKind() const { return kind_; }
  /// Checks if the value is of a specific kind.
  bool Is(Kind kind) const { return GetKind() == kind; }

  /// Replaces all uses of this value.
  void replaceAllUsesWith(Value *v);

  // Iterator over use sites.
  bool use_empty() const { return users_ == nullptr; }
  use_iterator use_begin() { return use_iterator(users_); }
  const_use_iterator use_begin() const { return const_use_iterator(users_); }
  use_iterator use_end() { return use_iterator(); }
  const_use_iterator use_end() const { return const_use_iterator(); }
  llvm::iterator_range<use_iterator> uses();
  llvm::iterator_range<const_use_iterator> uses() const;

  // Iterator over users.
  bool user_empty() const { return users_ == nullptr; }
  user_iterator user_begin() { return user_iterator(users_); }
  const_user_iterator user_begin() const { return const_user_iterator(users_); }
  user_iterator user_end() { return user_iterator(); }
  const_user_iterator user_end() const { return const_user_iterator(); }
  llvm::iterator_range<user_iterator> users();
  llvm::iterator_range<const_user_iterator> users() const;

  /// Do not allow assignments.
  void operator = (const Value &) = delete;
  /// Do not allow move assignments.
  void operator = (Value &&) = delete;

private:
  /// Use needs access to uses.
  friend class Use;
  /// Kind of the value.
  Kind kind_;
  /// Linked list of users.
  Use *users_;
};


/**
 * Value which references other values.
 */
class User {
public:
  using op_iterator = Use*;
  using const_op_iterator = const Use*;
  using op_range = llvm::iterator_range<op_iterator>;
  using const_op_range = llvm::iterator_range<const_op_iterator>;

  struct value_op_iterator : llvm::iterator_adaptor_base
      < value_op_iterator
      , op_iterator
      , std::random_access_iterator_tag
      , Value *
      , ptrdiff_t
      , Value *
      , Value *>
  {
    explicit value_op_iterator(Use *U = nullptr)
      : iterator_adaptor_base(U)
    {
    }

    Value *operator*() const { return *I; }
    Value *operator->() const { return *I; }
  };

  struct const_value_op_iterator : llvm::iterator_adaptor_base
      < const_value_op_iterator
      , const_op_iterator
      , std::random_access_iterator_tag
      , const Value *
      , ptrdiff_t
      , const Value *
      , const Value *>
  {
    explicit const_value_op_iterator(const Use *U = nullptr)
      : iterator_adaptor_base(U)
    {
    }

    const Value *operator*() const { return *I; }
    const Value *operator->() const { return *I; }
  };

  using value_op_range = llvm::iterator_range<value_op_iterator>;
  using const_value_op_range = llvm::iterator_range<const_value_op_iterator>;

public:
  /// Creates a new user.
  User(unsigned numOps);

  /// Cleans up after the use.
  virtual ~User();

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

  // Direct operand accessors.
  template <int Idx, typename U>
  static Use &OpFrom(const U *that)
  {
    auto *u = const_cast<U *>(that);
    return (Idx < 0 ? u->op_end() : u->op_begin())[Idx];
  }

  template <int Idx> Use &Op() { return OpFrom<Idx>(this); }
  template <int Idx> const Use &Op() const { return OpFrom<Idx>(this);}

protected:
  /// Grows the operand list.
  void growUses(unsigned n);

protected:
  /// Number of operands.
  unsigned numOps_;
  /// Head of the use list.
  Use *uses_;
};
