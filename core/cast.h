// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once



template <typename T>
typename std::enable_if
  < std::is_base_of<Value, T>::value &&
    std::is_same<Value::Kind, typename T::ParentType::Kind>::value
  , T *
  >::type
dyn_cast_or_null(Value *value)
{
  if (!value->Is(T::kKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}


template <typename T>
typename std::enable_if
  < std::is_base_of<Value, T>::value &&
    std::is_same<Value::Kind, typename T::ParentType::ParentType::Kind>::value
  , T *
  >::type
dyn_cast_or_null(Value *value)
{
  using ParentType = typename T::ParentType;
  if (!value->Is(ParentType::kKind)) {
    return nullptr;
  }
  if (!static_cast<ParentType *>(value)->Is(T::kKind)) {
    return nullptr;
  }
  return static_cast<T *>(value);
}
