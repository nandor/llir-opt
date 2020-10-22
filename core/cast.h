// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <type_traits>

#include "core/value.h"
#include "core/global.h"
#include "core/expr.h"
#include "core/constant.h"
#include "core/inst.h"
class Inst;
class CallSite;


namespace detail {

/**
 * Provides U with the constness of T.
 */
template <typename T, typename U>
struct copy_const {
  using type = typename std::conditional
      < std::is_const<T>::value
      , typename std::add_const<U>::type
      , U
      >::type;
};

/**
 * Helper template to identify the direct derivatives of value.
 */
template <typename T>
struct is_mutable_value : std::false_type {};

template<> struct is_mutable_value<Global> : std::true_type {};
template<> struct is_mutable_value<Constant> : std::true_type {};
template<> struct is_mutable_value<Expr> : std::true_type {};
template<> struct is_mutable_value<Inst> : std::true_type {};

template <typename T>
struct is_value {
  using TT = typename std::remove_const<T>::type;

  static constexpr bool value = is_mutable_value<TT>::value;
};

/**
 * Helper template to identify derivatives of inst.
 */
template <typename T, typename U>
struct is_class {
  using TT = typename std::remove_const<T>::type;

  static constexpr bool value =
      std::is_base_of<Value, TT>::value &&
      std::is_base_of<U, TT>::value &&
      !std::is_same<U, TT>::value;
};

}

/**
 * Casts a value to a direct subclass.
 */
template <typename T>
typename std::enable_if<detail::is_value<T>::value, T *>::type
cast_or_null(typename detail::copy_const<T, Value>::type *v)
{
  if (v == nullptr) {
    return nullptr;
  }
  if (v->Is(T::kValueKind)) {
    return static_cast<T *>(v);
  }
  return nullptr;
}

/**
 * Casts a value to an instruction.
 */
template<>
inline Inst *cast_or_null<Inst>(Value *v)
{
  if (v == nullptr) {
    return nullptr;
  }
  if ((reinterpret_cast<uintptr_t>(v) & 1) || v->Is(Inst::kValueKind)) {
    return static_cast<Inst *>(v);
  }
  return nullptr;
}

/**
 * Casts a value to an instruction.
 */
template<>
inline const Inst *cast_or_null<const Inst>(const Value *v)
{
  if (v == nullptr) {
    return nullptr;
  }
  if ((reinterpret_cast<uintptr_t>(v) & 1) || v->Is(Inst::kValueKind)) {
    return static_cast<const Inst *>(v);
  }
  return nullptr;
}

/**
 * Casts a value to an instruction.
 */
template <typename T>
typename std::enable_if<detail::is_class<T, Inst>::value, T *>::type
cast_or_null(typename detail::copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(Value::Kind::INST)) {
    return nullptr;
  }
  auto *inst = static_cast<typename detail::copy_const<T, Inst>::type *>(value);
  if (!inst->Is(T::kInstKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}

/**
 * Casts a value to a global.
 */
template <typename T>
typename std::enable_if<detail::is_class<T, Global>::value, T *>::type
cast_or_null(typename detail::copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(Value::Kind::GLOBAL)) {
    return nullptr;
  }
  if (!static_cast<const Global *>(value)->Is(T::kGlobalKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}

/**
 * Casts a value to a constant.
 */
template <typename T>
typename std::enable_if<detail::is_class<T, Constant>::value, T *>::type
cast_or_null(typename detail::copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(Value::Kind::CONST)) {
    return nullptr;
  }
  if (!static_cast<const Constant *>(value)->Is(T::kConstKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}


/**
 * Checked cast between pointers.
 */
template <typename T, typename U>
T *cast(U *from)
{
  if (T *t = ::cast_or_null<T>(from)) {
    return t;
  }
  llvm_unreachable("invalid dynamic type");
}

/**
 * Dynamic unchecked cast between references.
 */
template <typename T, typename U>
Ref<T> cast_or_null(Ref<U> from)
{
  return Ref(::cast_or_null<T>(from.Get()), from.Index());
}

/**
 * Dynamic unchecked cast between const references.
 */
template <typename T, typename U>
ConstRef<T> cast_or_null(ConstRef<U> from)
{
  return ConstRef(::cast_or_null<const T>(from.Get()), from.Index());
}

/**
 * Dynamic checked cast between references.
 */
template <typename T, typename U>
Ref<T> cast(Ref<U> from)
{
  return Ref(::cast<T>(from.Get()), from.Index());
}

/**
 * Dynamic checked cast between const references.
 */
template <typename T, typename U>
ConstRef<T> cast(ConstRef<U> from)
{
  return ConstRef(::cast<const T>(from.Get()), from.Index());
}
