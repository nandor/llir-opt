// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/ref.h"

class Use;
class User;
class Value;



/**
 * Use site of a value.
 */
class Use final {
public:
  /// Creates an empty use.
  Use() : val_(nullptr), user_(nullptr) {}
  /// Creates a new use in an object for a value.
  Use(Ref<Value> val, User *user) : val_(val), user_(user) {}
  /// Destroys a use.
  ~Use();

  /// Do not allow copy constructors.
  Use(const Use &) = delete;
  /// Do not allow move constructors.
  Use(Use &&) = delete;

  /// Assign a new value.
  Use &operator = (Ref<Value> val);

  /// Do not allow assignment.
  Use &operator = (const Use &) = delete;
  /// Do not allow move assignment.
  Use &operator = (Use &&) = delete;

  /// Returns the user attached to this use.
  User *getUser() const { return user_; }
  /// Returns the next use.
  Use *getNext() const { return next_; }

  // Return the underlying value.
  Ref<Value> get() { return val_; }
  // Return the underlying value.
  ConstRef<Value> get() const { return val_; }

  // Return the underlying value.
  operator Ref<Value>&() { return val_; }
  // Return the underlying value.
  operator ConstRef<Value>() const { return val_; }

  // Return the underlying value.
  Ref<Value> operator*() { return val_; }
  // Return the underlying value.
  ConstRef<Value> operator*() const { return val_; }
  // Point to the underlying value.
  Ref<Value> operator->() { return val_; }
  // Point to the underlying value.
  ConstRef<Value> operator->() const { return val_; }

  // Checks if the use points to a value.
  operator bool() const { return val_; }

private:
  friend class Value;
  friend class User;

  void Remove();
  void Add();

  /// Used value.
  Ref<Value> val_;
  /// Pointer to the user.
  User *user_;
  /// Previous use.
  Use *prev_ = nullptr;
  /// Next use.
  Use *next_ = nullptr;
};

