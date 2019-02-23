// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/iterator_range.h>

#include "passes/global_data_elim/bag.h"



/**
 * An item storing a constraint.
 */
class Constraint : public llvm::ilist_node<Constraint> {
protected:
  /**
   * Class to track uses of a constraint.
   */
  class Use {
  public:
    /// Creates a new reference to a value.
    Use(Constraint *user, Constraint *value)
      : user_(user)
      , value_(value)
    {
      if (value_) {
        next_ = value_->users_;
        prev_ = nullptr;
        if (next_) { next_->prev_ = this; }
        value_->users_ = this;
      } else {
        next_ = nullptr;
        prev_ = nullptr;
      }
    }

    /// Removes the value from the use chain.
    ~Use()
    {
      if (value_) {
        if (next_) { next_->prev_ = prev_; }
        if (prev_) { prev_->next_ = next_; }
        if (this == value_->users_) { value_->users_ = next_; }
        next_ = prev_ = nullptr;
      }
    }

    /// Returns the used value.
    operator Constraint * () const { return value_; }

    /// Returns the next use.
    Use *GetNext() const { return next_; }
    /// Returns the user.
    Constraint *GetUser() const { return user_; }

  private:
    friend class Constraint;
    /// User constraint.
    Constraint *user_;
    /// Used value.
    Constraint *value_;
    /// next_ item in the use chain.
    Use *next_;
    /// Previous item in the use chain.
    Use *prev_;
  };

  /**
   * Iterator over users.
   */
  template <typename UserTy>
  class iter_impl : public std::iterator<std::forward_iterator_tag, UserTy *> {
  public:
    // Preincrement
    iter_impl &operator++()
    {
      use_ = use_->GetNext();
      return *this;
    }

    // Postincrement
    iter_impl operator++(int)
    {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    // Retrieve a pointer to the current User.
    UserTy *operator*() const { return use_->GetUser(); }
    UserTy *operator->() const { return &operator*(); }

    // Iterator comparison.
    bool operator==(const iter_impl &x) const { return use_ == x.use_; }
    bool operator!=(const iter_impl &x) const { return use_ != x.use_; }

  private:
    friend class Constraint;

    iter_impl(Use *use) : use_(use) {}
    iter_impl() : use_(nullptr) {}

    Use *use_;
  };

  using iter = iter_impl<Constraint>;
  using const_iter = iter_impl<const Constraint>;

public:
  /// Enumeration of constraint kinds.
  enum class Kind {
    PTR,
    SUBSET,
    UNION,
    LOAD,
    STORE,
    CALL
  };

  /// Creates a new constraint.
  Constraint(Kind kind)
    : kind_(kind)
    , users_(nullptr)
  {
  }

  /// Deletes the constraint, remove it from use chains.
  virtual ~Constraint()
  {
    for (Use *use = users_; use; use = use->next_) {
      use->value_ = nullptr;
    }
  }

  /// Returns the node kind.
  Kind GetKind() const { return kind_; }
  /// Checks if the node is of a specific type.
  bool Is(Kind kind) { return GetKind() == kind; }

  // Iterator over users.
  bool empty() const { return users_ == nullptr; }
  iter begin() { return iter(users_); }
  const_iter begin() const { return const_iter(users_); }
  iter end() { return iter(); }
  const_iter end() const { return const_iter(); }
  llvm::iterator_range<iter> users() { return { begin(), end() }; }
  llvm::iterator_range<const_iter> users() const { return { begin(), end() }; }

private:
  /// The solver should access all fields.
  friend class ConstraintSolver;

  /// Kind of the node.
  Kind kind_;
  /// List of users.
  Use *users_;
};

// -----------------------------------------------------------------------------
class CPtr final : public Constraint {
public:
  CPtr(Bag *bag)
    : Constraint(Kind::PTR)
    , bag_(bag)
  {
  }

  /// Returns a pointer.
  Bag *GetBag() const { return bag_; }
  /// Checks if the bag is empty.
  bool IsEmpty() const { return bag_->IsEmpty(); }

private:
  /// Bag the pointer is pointing to.
  Bag *bag_;
};

class CSubset final : public Constraint {
public:
  /// Creates a subset constraint.
  CSubset(Constraint *subset, Constraint *set)
    : Constraint(Kind::SUBSET)
    , subset_(this, subset)
    , set_(this, set)
  {
  }

  /// Returns the subset.
  Constraint *GetSubset() const { return subset_; }
  /// Returns the set.
  Constraint *GetSet() const { return set_; }

private:
  /// Subset.
  Constraint::Use subset_;
  /// Set.
  Constraint::Use set_;
};

class CUnion final : public Constraint {
public:
  /// Creates a union constraint.
  CUnion(Constraint *lhs, Constraint *rhs)
    : Constraint(Kind::UNION)
    , lhs_(this, lhs)
    , rhs_(this, rhs)
  {
  }

  /// Returns the LHS of the union.
  Constraint *GetLHS() const { return lhs_; }
  /// Returns the RHS of the union.
  Constraint *GetRHS() const { return rhs_; }

private:
  /// LHS of the union.
  Constraint::Use lhs_;
  /// RHS of the union.
  Constraint::Use rhs_;
};

class CLoad final : public Constraint {
public:
  /// Creates a new load constraint.
  CLoad(Constraint *ptr)
    : Constraint(Kind::LOAD)
    , ptr_(this, ptr)
    , valSet_(std::make_unique<Bag>())
    , ptrSet_(std::make_unique<Bag>())
  {
  }

  /// Returns the pointer.
  Constraint *GetPointer() const { return ptr_; }

  /// Returns the set of possible values.
  Bag *GetValSet() { return valSet_.get(); }
  /// Returns the set of known pointers.
  Bag *GetPtrSet() { return ptrSet_.get(); }

private:
  /// Dereferenced pointer.
  Constraint::Use ptr_;
  /// Set of values.
  std::unique_ptr<Bag> valSet_;
  /// Set of pointers.
  std::unique_ptr<Bag> ptrSet_;
};

class CStore final : public Constraint {
public:
  /// Creates a new store constraint.
  CStore(Constraint *val, Constraint *ptr)
    : Constraint(Kind::STORE)
    , val_(this, val)
    , ptr_(this, ptr)
    , valSet_(std::make_unique<Bag>())
    , ptrSet_(std::make_unique<Bag>())
  {
  }

  /// Returns the value.
  Constraint *GetValue() const { return val_; }
  /// Returns the pointer.
  Constraint *GetPointer() const { return ptr_; }

  /// Returns the set of possible values.
  Bag *GetValSet() { return valSet_.get(); }
  /// Returns the set of possible pointers.
  Bag *GetPtrSet() { return ptrSet_.get(); }

private:
  /// Value to write.
  Constraint::Use val_;
  /// Pointer written to.
  Constraint::Use ptr_;
  /// Set of values.
  std::unique_ptr<Bag> valSet_;
  /// Set of pointers.
  std::unique_ptr<Bag> ptrSet_;
};

class CCall final : public Constraint {
public:
  /// Creates a new call constraint.
  CCall(
      std::vector<Inst *> &context,
      Constraint *callee,
      std::vector<Constraint *> &args)
    : Constraint(Kind::CALL)
    , context_(context)
    , nargs_(args.size())
    , callee_(this, callee)
    , args_(static_cast<Constraint::Use *>(malloc(sizeof(Constraint::Use) * nargs_)))
    , ptrSet_(std::make_unique<Bag>())
    , retSet_(std::make_unique<Bag>())
  {
    for (unsigned i = 0; i < args.size(); ++i) {
      new (&args_[i]) Constraint::Use(this, args[i]);
    }
  }

  /// Returns the callee name.
  const std::vector<Inst *> &GetContext() const { return context_; }
  /// Returns the callee.
  Constraint *GetCallee() const { return callee_; }
  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return nargs_; }
  /// Returns the ith argument.
  Constraint *GetArg(unsigned i) const { return args_[i]; }

  /// Returns the set of possible pointers.
  Bag *GetPtrSet() { return ptrSet_.get(); }
  /// Returns the set of possible pointers.
  Bag *GetRetSet() { return retSet_.get(); }

private:
  /// Callee context.
  std::vector<Inst *> context_;
  /// Number of args.
  unsigned nargs_;
  /// Callee.
  Constraint::Use callee_;
  /// Arguments.
  Constraint::Use *args_;

  /// Set of pointers.
  std::unique_ptr<Bag> ptrSet_;
  /// Set of return values.
  std::unique_ptr<Bag> retSet_;
};
