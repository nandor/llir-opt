// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/adt/id.h"

class CallSite;
class Object;
class SymbolicObject;



/**
 * Mapping from objects to object IDs.
 */
class SymbolicHeap final {
public:
  /// Class to describe the origin of an object.
  class Origin {
  public:
    /// Enumeration of object kinds.
    enum class Kind {
      DATA,
      FRAME,
      ALLOC,
    };

    /// Return the kind of the object.
    Kind GetKind() const { return kind_; }

  public:
    /// ID of the object kind.
    Kind kind_;
  };


public:
  /// Initialise the mapping.
  SymbolicHeap() : next_(0u) {}

  /// Record an ID for an object.
  ID<SymbolicObject> Data(Object *object);
  /// Record an ID for a frame.
  ID<SymbolicObject> Frame(unsigned frame, unsigned object);
  /// Record an ID for an allocation.
  ID<SymbolicObject> Alloc(unsigned frame, CallSite *site);

  /// Returns the origin of an object.
  Origin &Map(ID<SymbolicObject> id) { return *origins_[id]; }

private:
  /// Next available ID.
  unsigned next_;
  /// Mapping from objects to IDs.
  std::unordered_map<Object *, ID<SymbolicObject>> objects_;
  /// Mapping from frame objects to IDs.
  std::unordered_map<std::pair<unsigned, unsigned>, ID<SymbolicObject>> frames_;
  /// Mapping from allocations to IDs.
  std::unordered_map<std::pair<unsigned, CallSite *>, ID<SymbolicObject>> allocs_;
  /// Mapping from IDs to origins.
  std::vector<std::unique_ptr<Origin>> origins_;
};
