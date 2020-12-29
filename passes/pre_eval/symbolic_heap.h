// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <set>
#include <unordered_map>

class Atom;
class Prog;
class Object;



/**
 * Object in the abstract heap.
 */
class SymbolicObject final {
public:
  /// Creates the symbolic representation of the object.
  SymbolicObject(Object &object);
  /// Cleanup.
  ~SymbolicObject();

  /**
   * Performs a store to an atom inside the object.
   */
  void StoreAtom(
      Atom *a,
      int64_t offset,
      const SymbolicValue &val,
      Type type
  );

private:
  /// Reference to the object represented here.
  Object &object_;
  /// Base alignment of the object.
  llvm::Align align_;
  /// Set of pointer-sized buckets.
  std::vector<SymbolicValue> buckets_;
  /// Start bucket and offset into a bucket.
  std::unordered_map<Atom *, std::pair<unsigned, unsigned>> start_;
  /// Size of the modelled part.
  size_t size_;
};

/**
 * Symbolic representation of the heap.
 */
class SymbolicHeap final {
public:
  /// Creates a new heap using values specified in the data segments.
  SymbolicHeap(Prog &prog);

  /// Cleanup.
  ~SymbolicHeap();

  /**
   * Stores a value to the symbolic heap representation.
   *
   * If the value is stored to a precise location, the heap is updated to
   * reflect the result of the store. Otherwise, the whole range of addresses
   * is invalidated in order to over-approximate unknown stores.
   */
  void Store(
      const SymbolicPointer &addr,
      const SymbolicValue &val,
      Type type
  );

private:
  /**
   * Performs a store to a precise pointer.
   */
  void StoreGlobal(
      Global *g,
      int64_t offset,
      const SymbolicValue &val,
      Type type
  );

  /**
   * Performs a store to an atom.
   */
  void StoreAtom(
      Atom *a,
      int64_t offset,
      const SymbolicValue &val,
      Type type
  );

  /**
   * Taints the store due to an imprecise location.
   */
  void StoreImprecise(const SymbolicPointer &addr);

  /**
   * Performs a store to an external pointer.
   */
  void StoreExtern(const SymbolicValue &val);

private:
  /// Mapping from heap-allocated objects to their symbolic values.
  std::unordered_map<Object *, std::unique_ptr<SymbolicObject>> objects_;
};
