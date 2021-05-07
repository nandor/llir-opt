// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



/**
 * Helper class to build custom visitors for all IR instructions.
 */
template<typename T>
class InstVisitor {
public:
  virtual ~InstVisitor() {}

  T Dispatch(Inst &i)
  {
    switch (i.GetKind()) {
      #define GET_INST(kind, type, name, sort) \
        case Inst::Kind::kind: return Visit##type(static_cast<type &>(i));
      #include "instructions.def"
    }
    llvm_unreachable("invalid instruction kind");
  }

public:
  virtual T VisitInst(Inst &i) = 0;

public:
  #define GET_KIND(base, sort) \
    virtual T Visit##base(base &i) { return Visit##sort(i); }
  #include "instructions.def"

  #define GET_INST(kind, type, name, sort) \
    virtual T Visit##type(type &i) { return Visit##sort(i); }
  #include "instructions.def"
};

/**
 * Helper class to build custom visitors for all IR instructions.
 */
template<typename T>
class ConstInstVisitor {
public:
  virtual ~ConstInstVisitor() {}

  T Dispatch(const Inst &i)
  {
    switch (i.GetKind()) {
      #define GET_INST(kind, type, name, sort) \
        case Inst::Kind::kind: return Visit##type(static_cast<const type &>(i));
      #include "instructions.def"
    }
    llvm_unreachable("invalid instruction kind");
  }

public:
  virtual T VisitInst(const Inst &i) = 0;

public:
  #define GET_KIND(base, sort) \
    virtual T Visit##base(const base &i) { return Visit##sort(i); }
  #include "instructions.def"

  #define GET_INST(kind, type, name, sort) \
    virtual T Visit##type(const type &i) { return Visit##sort(i); }
  #include "instructions.def"
};
