// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <iterator>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/iterator_range.h>

#include "core/use.h"

template<typename T> class Ref;



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
  virtual void replaceAllUsesWith(Value *v);
  /// Replaces all uses of this with a refernce.
  virtual void replaceAllUsesWith(Ref<Value> v);

  // Iterator over use sites.
  size_t use_size() const { return std::distance(use_begin(), use_end()); }
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

