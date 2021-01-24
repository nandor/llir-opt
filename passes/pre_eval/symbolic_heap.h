// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/adt/id.h"

class CallSite;
class Object;
class SymbolicObject;
class Func;



/**
 * Mapping from objects to object IDs.
 */
class SymbolicHeap final {
public:
  /// Class to describe the origin of an object.
  class Origin final {
  public:
    /// Enumeration of object kinds.
    enum class Kind {
      DATA,
      FRAME,
      ALLOC,
    };

    /// Data object.
    struct DataOrigin {
      Kind K;
      Object *Obj;

      DataOrigin(Object *obj)
        : K(Kind::DATA)
        , Obj(obj)
      {
      }
    };

    /// Frame object.
    struct FrameOrigin {
      Kind K;
      unsigned Frame;
      unsigned Index;

      FrameOrigin(unsigned frame, unsigned index)
        : K(Kind::FRAME)
        , Frame(frame)
        , Index(index)
      {
      }
    };

    /// Heap object.
    struct AllocOrigin {
      Kind K;
      unsigned Frame;
      CallSite *Alloc;

      AllocOrigin(unsigned frame, CallSite *alloc)
        : K(Kind::ALLOC)
        , Frame(frame)
        , Alloc(alloc)
      {
      }
    };

    /// Return the kind of the object.
    Kind GetKind() const { return v_.K; }

    /// Return the data origin.
    DataOrigin &AsData() { return v_.D; }
    /// Return the frame origin.
    FrameOrigin &AsFrame() { return v_.F; }
    /// Return the alloc origin.
    AllocOrigin &AsAlloc() { return v_.A; }

  public:
    template <typename... Args>
    Origin(Args... args) : v_(std::forward<Args>(args)...) {}

  public:
    /// ID of the object kind.
    union U {
      Kind K;
      DataOrigin D;
      FrameOrigin F;
      AllocOrigin A;

      U(Object *object) { new (&D) DataOrigin(object); }
      U(unsigned fr, unsigned idx) { new (&F) FrameOrigin(fr, idx); }
      U(unsigned fr, CallSite *alloc) { new (&A) AllocOrigin(fr, alloc); }
    } v_;
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
  /// Record an ID for a function.
  ID<Func> Function(Func *f);

  /// Returns the origin of an object.
  Origin &Map(ID<SymbolicObject> id) { return origins_[id]; }
  /// Returns the origin of a func.
  Func &Map(ID<Func> id);

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
  std::vector<Origin> origins_;
  /// Mapping from functions to IDs.
  std::unordered_map<Func *, ID<Func>> funcToIDs_;
  /// Mapping from IDs to functions.
  std::vector<Func *> idToFunc_;
};
