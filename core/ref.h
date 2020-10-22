// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/adt/hash.h"
#include "core/type.h"
#include "core/use.h"

class Use;
class User;
class Value;
class Inst;
class Global;
class Use;



/**
 * Base class for all references.
 */
template <typename T>
class RefBase {
public:
  T *Get() const { return inst_; }
  T *operator->() const { return inst_; }
  T &operator*() const { return *inst_; }

  unsigned Index() const { return index_; }

  template <typename U = T>
  typename std::enable_if<std::is_base_of<Inst, U>::value, Type>::type
  GetType() const
  {
    return inst_->GetType(index_);
  }

  operator bool() const { return inst_ != nullptr; }

  bool operator==(const RefBase<T> &that) const
  {
    return inst_ == that.inst_ && index_ == that.index_;
  }

  bool operator!=(const RefBase<T> &that) const
  {
    return !(*this == that);
  }

protected:
  RefBase(T *inst, unsigned index) : inst_(inst), index_(index) {}

protected:
  /// Instruction pointed to.
  T *inst_;
  /// Index of the return value in the instruction.
  unsigned index_;
};

/**
 * Reference to an instruction or a value.
 */
template <typename T>
class Ref : public RefBase<T> {
public:
  Ref() : RefBase<T>(nullptr, 0) {}
  Ref(T *inst, unsigned idx = 0) : RefBase<T>(inst, idx) {}

  template
    < typename U
    , class = typename std::enable_if<std::is_base_of<U, T>::value>::type
    >
  operator Ref<U>() const
  {
    return Ref<U>(static_cast<U *>(this->inst_), this->index_);
  }
};

/**
 * Constant reference to an instruction or a value.
 */
template <typename T>
class ConstRef : public RefBase<const T> {
public:
  ConstRef() : RefBase<const T>(nullptr, 0) {}
  ConstRef(const T *inst, unsigned idx = 0) : RefBase<const T>(inst, idx) {}
  ConstRef(const Ref<T> &ref) : RefBase<const T>(ref.Get(), ref.Index()) {}

  template
    < typename U
    , class = typename std::enable_if<std::is_base_of<U, T>::value>::type
    >
  operator ConstRef<U>() const
  {
    return {static_cast<const U *>(this->inst_), this->index_};
  }
};

/**
 * Hashing function for references.
 */
template<typename T>
struct std::hash<Ref<T>> {
  std::size_t operator()(const Ref<T> &ref) const
  {
    size_t hash = 0;
    ::hash_combine(hash, ref.Get());
    ::hash_combine(hash, ref.Index());
    return hash;
  }
};

/**
 * Hashing function for constant references.
 */
template<typename T>
struct std::hash<ConstRef<T>> {
  std::size_t operator()(const ConstRef<T> &ref) const
  {
    size_t hash = 0;
    ::hash_combine(hash, ref.Get());
    ::hash_combine(hash, ref.Index());
    return hash;
  }
};

