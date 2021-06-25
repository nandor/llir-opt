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
  auto sl = Find(i.GetLHS()), sr = Find(i.GetRHS()), so = Find(i);

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
        return Alternatives(i, {
            { IsPtr(sl), { IsInt(sr) } },
            { IsInt(sl), { IsInt(sr) } }
        });
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
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl) } }
        });
      }
      if (vl.IsInt() && vr.IsPtrUnion()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sl) } }
        });
      }
      if (vl.IsPtrLike() && vr.IsPtrUnion()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl), IsInt(sr) } },
            { IsPtr(so), { IsInt(sl), IsPtr(sr) } }
        });
      }

      i.getParent()->dump();
      llvm::errs() << "\n" << i << "\n\n";
      llvm_unreachable("invalid add type");
    }
  }
  llvm_unreachable("unknown value kind");
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitSubInst(SubInst &i)
{
  Infer(i);
  return;

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  auto sl = Find(i.GetLHS()), sr = Find(i.GetRHS()), so = Find(i);

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
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsPtrUnion()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl), IsInt(sr) } }
        });
      }
      if (vl.IsPtrLike() && vr.IsPtrUnion()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sr) } }
        });
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
  return;

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  auto sl = Find(i.GetLHS()), sr = Find(i.GetRHS()), so = Find(i);

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
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl), IsInt(sr), } },
            { IsPtr(so), { IsInt(sl), IsPtr(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl) } }
        });
      }
      if (vr.IsPtrUnion() && vl.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sl) } }
        });
      }
      if (vr.IsPtrUnion() && vl.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sr) } }
        });
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
  return;
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  auto sl = Find(i.GetLHS()), sr = Find(i.GetRHS()), so = Find(i);

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
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl), } },
            { IsPtr(so), { IsPtr(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl) } }
        });
      }
      if (vr.IsPtrUnion() && vl.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), {} }
        });
      }
      if (vr.IsPtrUnion() && vl.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), {} }
        });
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
  return;

  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto vo = analysis_.Find(i);
  auto sl = Find(i.GetLHS()), sr = Find(i.GetRHS()), so = Find(i);

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
        return Alternatives(i, {
            { IsPtr(sl), { IsInt(sr) } },
            { IsInt(sl), { IsPtr(sr) } }
        });
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
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl), IsInt(sr) } },
            { IsPtr(so), { IsInt(sl), IsPtr(sr) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sl) } }
        });
      }
      if (vl.IsPtrUnion() && vr.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sl) } }
        });
      }
      if (vr.IsPtrUnion() && vl.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sr) } }
        });
      }
      if (vr.IsPtrUnion() && vl.IsPtrLike()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sr) } }
        });
      }
      if (vl.IsPtrLike() && vr.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsInt(sr) } }
        });
      }
      if (vr.IsPtrLike() && vl.IsInt()) {
        return Alternatives(i, {
            { IsInt(so), {} },
            { IsPtr(so), { IsPtr(sr) } }
        });
      }
      llvm_unreachable("invalid and kind");
    }
  }
  llvm_unreachable("unknown value kind");
}
