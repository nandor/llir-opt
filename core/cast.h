// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once



template <typename T>
typename std::enable_if
  < std::is_base_of<Value, T>::value && (
      std::is_same<Inst, T>::value ||
      std::is_same<Global, T>::value
    )
  , T *
  >::type
dyn_cast_or_null(Value *value)
{
  if (!value->Is(T::kValueKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}


template <typename T>
typename std::enable_if
  < std::is_base_of<Value, T>::value &&
    std::is_base_of<Inst, T>::value &&
    !std::is_same<Inst, T>::value
  , T *
  >::type
dyn_cast_or_null(Value *value)
{
  if (!value->Is(Value::Kind::INST)) {
    return nullptr;
  }
  if (!static_cast<Inst *>(value)->Is(T::kInstKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}


template <typename T>
typename std::enable_if
  < std::is_base_of<Value, T>::value &&
    std::is_base_of<Global, T>::value &&
    !std::is_same<Global, T>::value
  , T *
  >::type
dyn_cast_or_null(Value *value)
{
  if (!value->Is(Value::Kind::GLOBAL)) {
    return nullptr;
  }
  if (!static_cast<Global *>(value)->Is(T::kGlobalKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}
