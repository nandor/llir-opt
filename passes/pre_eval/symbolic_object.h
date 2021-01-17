// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/Alignment.h>

#include "core/type.h"
#include "core/func.h"

#include "passes/pre_eval/symbolic_value.h"

class Atom;
class Prog;
class Object;
class SymbolicFrame;
class SymbolicValue;
class SymbolicHeap;



/**
 * Object in the abstract heap.
 */
class SymbolicObject final {
public:
  /// Iterator over buckets.
  using bucket_iterator = std::vector<SymbolicValue>::const_iterator;

public:
  /// Constructs a symbolic object.
  SymbolicObject(
      ID<SymbolicObject> id,
      std::optional<size_t> size,
      llvm::Align align,
      bool rdonly
  );
  /// Cleanup.
  ~SymbolicObject();

  /// Return the ID of the object.
  ID<SymbolicObject> GetID() const { return id_; }
  /// Return the alignment.
  llvm::Align GetAlignment() const { return align_; }

  /// Iterator over buckets.
  bucket_iterator begin() const { return buckets_.begin(); }
  bucket_iterator end() const { return buckets_.end(); }

  /// Merges another object into this one.
  void LUB(const SymbolicObject &that);

  /// Initialises a value inside the object.b
  bool Init(int64_t offset, const SymbolicValue &val, Type type);
  /// Performs a store to an atom inside the object.
  bool Store(int64_t offset, const SymbolicValue &val, Type type);
  /// Performs a load from an atom inside the object.
  SymbolicValue Load(int64_t offset, Type type);
  /// Clobbers the value at an exact location.
  bool StoreImprecise(int64_t offset, const SymbolicValue &val, Type type);
  /// Stores a value to an unknown location in the object.
  bool StoreImprecise(const SymbolicValue &val, Type type);
  /// Reads a value from all possible locations in the object.
  SymbolicValue LoadImprecise(Type type);

private:
  /// Stores to the object.
  bool WritePrecise(int64_t offset, const SymbolicValue &val, Type type);
  /// Loads from the object.
  SymbolicValue ReadPrecise(int64_t offset, Type type);
  /// Stores to the object without knowing the actual value.
  bool WriteImprecise(int64_t offset, const SymbolicValue &val, Type type);
  /// Stores to the object with a given mutator.
  bool Write(
      int64_t offset,
      const SymbolicValue &val,
      Type type,
      bool (SymbolicObject::*mutate)(unsigned, const SymbolicValue &)
  );

  /// Sets a value in a bucket.
  bool Set(unsigned bucket, const SymbolicValue &val);
  /// Unifies a value in a bucket.
  bool Merge(unsigned bucket, const SymbolicValue &val);
  /// Set the approximate value.
  bool Merge(const SymbolicValue &value);

private:
  /// Identifier of the object.
  ID<SymbolicObject> id_;
  /// Size of the underlying object.
  std::optional<size_t> size_;
  /// Base alignment of the object.
  llvm::Align align_;
  /// Set of pointer-sized buckets.
  std::vector<SymbolicValue> buckets_;
  /// Flag to indicate whether the object can be writen.
  bool rdonly_;
};
