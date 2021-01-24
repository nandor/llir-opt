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
  /// Constructs a symbolic object.
  SymbolicObject(
      ID<SymbolicObject> id,
      std::optional<size_t> size,
      llvm::Align align,
      bool rdonly,
      bool zero
  );
  /// Copies a symbolic object.
  SymbolicObject(const SymbolicObject &that);
  /// Cleanup.
  ~SymbolicObject();

  /// Return the ID of the object.
  ID<SymbolicObject> GetID() const { return id_; }
  /// Return the alignment.
  llvm::Align GetAlignment() const { return align_; }

  /// Iterator over buckets.
  const SymbolicValue *begin() const;
  const SymbolicValue *end() const;

  /// Merges another object into this one.
  void Merge(const SymbolicObject &that);

  /// Performs a load from an atom inside the object.
  SymbolicValue Load(int64_t offset, Type type);
  /// Reads a value from all possible locations in the object.
  SymbolicValue LoadImprecise(Type type);

  /// Initialises a value inside the object.b
  bool Init(int64_t offset, const SymbolicValue &val, Type type);
  /// Performs a store to an atom inside the object.
  bool Store(int64_t offset, const SymbolicValue &val, Type type);
  /// Clobbers the value at an exact location.
  bool StoreImprecise(int64_t offset, const SymbolicValue &val, Type type);
  /// Stores a value to an unknown location in the object.
  bool StoreImprecise(const SymbolicValue &val, Type type);

private:
  /// Identifier of the object.
  ID<SymbolicObject> id_;
  /// Size of the underlying object, if known.
  std::optional<size_t> size_;
  /// Base alignment of the object.
  llvm::Align align_;
  /// Flag to indicate whether the object can be writen.
  bool rdonly_;

  /// Inaccurate storage.
  class MergedStorage {
  public:
    /// Initialises storage to a specific value.
    MergedStorage(const SymbolicValue &value) : value_(value) {}

    /// Return the approximation.
    SymbolicValue Load() const { return value_; }

    /// Merge a new value into the approximation.
    bool Store(const SymbolicValue &value);

    /// Iterator over buckets.
    const SymbolicValue *begin() const { return &value_; }
    const SymbolicValue *end() const { return &value_ + 1; }

  private:
    /// Flag to indicate whether object is accurate.
    bool Accurate = false;
    /// Underlying storage, LUB of all values stored.
    SymbolicValue value_;
  };

  /// Accurate storage, up to a limit.
  class BucketStorage {
  public:
    /// Initialises the bucket storage.
    BucketStorage(size_t size, const SymbolicValue &value);

    /// Merge in another bucket.
    void Merge(const BucketStorage &that);

    SymbolicValue Load(int64_t offset, Type type) const;
    SymbolicValue Load() const { return approx_; }

    bool StorePrecise(int64_t offset, const SymbolicValue &value, Type type);
    bool StoreImprecise(int64_t offset, const SymbolicValue &value, Type type);

    /// Iterator over buckets.
    const SymbolicValue *begin() const { return &*buckets_.begin(); }
    const SymbolicValue *end() const { return &*buckets_.end(); }

  private:
    /// Reads from a precise location.
    SymbolicValue Read(int64_t offset, Type type) const;
    /// Stores to the object with a given mutator.
    bool Write(
        int64_t offset,
        const SymbolicValue &val,
        Type type,
        bool (BucketStorage::*mutate)(unsigned, const SymbolicValue &)
    );
    /// Sets a value in a bucket.
    bool Set(unsigned bucket, const SymbolicValue &val);
    /// Unifies a value in a bucket.
    bool Merge(unsigned bucket, const SymbolicValue &val);

  private:
    bool Accurate = true;
    /// Buckets used for storage.
    std::vector<SymbolicValue> buckets_;
    /// Additional approximation.
    SymbolicValue approx_;
  };

  /// Union of the two storage mechanisms.
  union U {
    bool Accurate;
    MergedStorage M;
    BucketStorage B;

    U() { }
    ~U() { }
  } v_;
};
