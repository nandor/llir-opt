// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
void ConstraintSolver::VisitAddInst(AddInst &i)
{
  Infer(i);

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);

  switch (vo.GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::UNDEF: {
      // Type is well-defined, no refinement possible.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      if ((vl.IsPtrLike() && vr.IsInt()) || (vr.IsPtrLike() || vl.IsInt())) {
        // No ambiguity.
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vl == ptr && vr == int
        //  OR
        // vl == int && vr == ptr
        //  OR
        // vl == ptr|int && vr == ptr|int
        return;
      }
      llvm_unreachable("invalid add type");
    }
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT:
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      if (vl.IsPtrUnion() && vr.IsInt()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        //  OR
        // vo == ptr|int && vl == ptr|int
        return;
      }
      if (vl.IsInt() && vr.IsPtrUnion()) {
        // vo == int && (vr == ptr|int)
        //  OR
        // vo == ptr && vr == ptr && vl == int
        //  OR
        // vo == ptr|int && vr == ptr|int
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == int
        //  OR
        // vo == ptr|int && vl == ptr|int
        return;
      }
      if (vl.IsPtrLike() && vr.IsPtrUnion()) {
        // vo == int && vr == ptr|int
        //  OR
        // vo == ptr && vr == int
        //  OR
        // vo == ptr|int && vr == ptr|int
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vo == int && vr == ptr|int && vl == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        //  OR
        // vo == ptr && vl == int && vr == ptr
        //  OR
        // vo == ptr|int && vl == ptr|int && vr == ptr|int
        return;
      }
      llvm_unreachable("invalid add type");
    }
  }
  llvm_unreachable("unknown value kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSubInst(SubInst &i)
{
  Infer(i);

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);

  switch (vo.GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::UNDEF: {
      // Type is well-defined, no refinement possible.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      if (vl.IsPtrLike() && vr.IsInt()) {
        // No ambiguity.
        return;
      }
      llvm_unreachable("invalid sub type");
    }
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT:
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      if (vl.IsPtrUnion() && vr.IsInt()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == ptr
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        return;
      }
      if (vl.IsPtrLike() && vr.IsPtrUnion()) {
        // vo == int && vr == ptr|int
        //  OR
        // vo == ptr && vr == int
        return;
      }
      llvm_unreachable("invalid sub type");
    }
  }
  llvm_unreachable("unknown value kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitOrInst(OrInst &i)
{
  Infer(i);

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);

  switch (vo.GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::UNDEF: {
      // Type is well-defined, no refinement possible.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      if (vl.IsPtrLike() || vr.IsPtrLike()) {
        // Nothing to refine.
        return;
      }
      llvm_unreachable("invalid or kind");
    }
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT:
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      if ((vl.IsPtrLike() && vr.IsInt()) || (vr.IsPtrLike() && vl.IsInt())) {
        // No refinement possible without knowledge of integers.
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        //  OR
        // vo == ptr && vl == int && vr == ptr
        return;
      }
      if (vl.IsPtrUnion() && vr.IsInt()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == ptr
        return;
      }
      if (vr.IsPtrUnion() && vl.IsInt()) {
        // vo == int && vr == ptr|int
        //  OR
        // vo == ptr && vr == ptr
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == int
        return;
      }
      if (vr.IsPtrUnion() && vl.IsPtrLike()) {
        // vo == int && vr == ptr|int
        //  OR
        // vo == ptr && vr == int
        return;
      }
      llvm_unreachable("invalid or kind");
    }
  }
  llvm_unreachable("unknown value kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitAndInst(AndInst &i)
{
  Infer(i);

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);

  switch (vo.GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::UNDEF: {
      // Type is well-defined, no refinement possible.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      if ((vl.IsPtrLike() && vr.IsInt()) || (vl.IsInt() && vr.IsPtrLike())) {
        return;
      }
      llvm_unreachable("invalid and kind");
    }
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT:
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      if ((vl.IsPtrLike() && vr.IsInt()) || (vr.IsPtrLike() && vl.IsInt())) {
        // No refinement possible without knowledge of integers.
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr|int && vr == ptr
        return;
      }
      if (vl.IsPtrUnion() && vr.IsInt()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == ptr
        return;
      }
      if (vr.IsPtrUnion() && vl.IsInt()) {
        // vo == int && vr == ptr|int
        //  OR
        // vo == ptr && vr == ptr
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        // vo == int && vl == ptr|int
        //  OR
        // vo == ptr && vl == ptr|int
        return;
      }
      if (vr.IsPtrUnion() && vl.IsPtrLike()) {
        // vo == int && vr == ptr|int
        //  OR
        // vo == ptr && vr == ptr|int
        return;
      }
      llvm_unreachable("invalid and kind");
    }
  }
  llvm_unreachable("unknown value kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitXorInst(XorInst &i)
{
  Infer(i);

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);

  switch (vo.GetKind()) {
    case TaggedType::Kind::UNKNOWN:
    case TaggedType::Kind::INT:
    case TaggedType::Kind::FUNC:
    case TaggedType::Kind::UNDEF: {
      // Type is well-defined, no refinement possible.
      return;
    }
    case TaggedType::Kind::YOUNG:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::ADDR:
    case TaggedType::Kind::PTR: {
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vl == ptr && vr == int
        //  OR
        // vl == int && vr == ptr
        return;
      }
      llvm_unreachable("invalid xor kind");
    }
    case TaggedType::Kind::HEAP_OFF:
    case TaggedType::Kind::ADDR_NULL:
    case TaggedType::Kind::ADDR_INT:
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::PTR_NULL:
    case TaggedType::Kind::PTR_INT: {
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        //  OR
        // vo == ptr && vl == int && vr == ptr
        return;
      }
      if (vl.IsPtrUnion() && vr.IsInt()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        return;
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == int && vr == ptr
        return;
      }
      if (vr.IsPtrUnion() && vl.IsInt()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == int && vr == ptr
        return;
      }
      if (vr.IsPtrUnion() && vl.IsPtrLike()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        return;
      }
      if (vl.IsPtrLike() && vr.IsInt()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == ptr && vr == int
        return;
      }
      if (vr.IsPtrLike() && vl.IsInt()) {
        // vo == int && vl == ptr|int && vr == ptr|int
        //  OR
        // vo == ptr && vl == int && vr == ptr
        return;
      }
      llvm_unreachable("invalid and kind");
    }
  }
  llvm_unreachable("unknown value kind");
}
