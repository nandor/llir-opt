// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <type_traits>

template <typename T, typename U>
struct copy_const {
  using type = typename std::conditional
      < std::is_const<T>::value
      , typename std::add_const<U>::type
      , U
      >::type;
};

template <typename T>
typename std::enable_if
  < std::is_base_of<Value, T>::value && (
      std::is_same<Inst, typename std::remove_const<T>::type>::value ||
      std::is_same<Global, typename std::remove_const<T>::type>::value ||
      std::is_same<Constant, typename std::remove_const<T>::type>::value
    )
  , T *
  >::type
dyn_cast_or_null(typename copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(T::kValueKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}


template <typename T>
typename std::enable_if
  < std::is_base_of<Value, typename std::remove_const<T>::type>::value &&
    std::is_base_of<Inst, typename std::remove_const<T>::type>::value &&
    !std::is_same<Inst, typename std::remove_const<T>::type>::value
  , T *
  >::type
dyn_cast_or_null(typename copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(Value::Kind::INST)) {
    return nullptr;
  }
  auto *inst = static_cast<typename copy_const<T, Inst>::type *>(value);
  if (!inst->Is(T::kInstKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}


template <typename T>
typename std::enable_if
  < std::is_base_of<Value, typename std::remove_const<T>::type>::value &&
    std::is_base_of<Global, typename std::remove_const<T>::type>::value &&
    !std::is_same<Global, typename std::remove_const<T>::type>::value
  , T *
  >::type
dyn_cast_or_null(typename copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(Value::Kind::GLOBAL)) {
    return nullptr;
  }
  if (!static_cast<Global *>(value)->Is(T::kGlobalKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}

template <typename T>
typename std::enable_if
  < std::is_base_of<Value, typename std::remove_const<T>::type>::value &&
    std::is_base_of<Constant, typename std::remove_const<T>::type>::value &&
    !std::is_same<Constant, typename std::remove_const<T>::type>::value
  , T *
  >::type
dyn_cast_or_null(typename copy_const<T, Value>::type *value)
{
  if (value == nullptr) {
    return nullptr;
  }
  if (!value->Is(Value::Kind::CONST)) {
    return nullptr;
  }
  if (!static_cast<Constant *>(value)->Is(T::kConstKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}
