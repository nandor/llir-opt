// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/prog.h"
#include "passes/tags/constraints.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver(
    RegisterAnalysis &analysis,
    const Target *target,
    Prog &prog)
  : analysis_(analysis)
  , target_(target)
  , prog_(prog)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Solve()
{
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        Dispatch(inst);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitInst(Inst &i)
{
  std::string msg;
  llvm::raw_string_ostream os(msg);
  os << i << "\n";
  llvm::report_fatal_error(msg.c_str());
}

// -----------------------------------------------------------------------------
ID<ConstraintSolver::Constraint> ConstraintSolver::Find(Ref<Inst> a)
{
  if (auto it = ids_.find(a); it != ids_.end()) {
    return union_.Find(it->second);
  } else {
    auto id = union_.Emplace();
    ids_.emplace(a, id);
    return id;
  }
}

// -----------------------------------------------------------------------------
ConstraintSolver::Constraint *ConstraintSolver::Map(Ref<Inst> a)
{
  return union_.Map(Find(a));
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Subset(Ref<Inst> from, Ref<Inst> to)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtMost(Ref<Inst> a, const TaggedType &type)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtLeast(Ref<Inst> a, const TaggedType &type)
{
}

// -----------------------------------------------------------------------------
void ConstraintSolver::AtMostInfer(Ref<Inst> arg)
{
  auto type = analysis_.Find(arg);
  if (type.IsUnknown()) {
    switch (auto ty = arg.GetType()) {
      case Type::V64: {
        return AtMost(arg, TaggedType::Val());
      }
      case Type::I8:
      case Type::I16:
      case Type::I32:
      case Type::I64:
      case Type::I128: {
        if (target_->GetPointerType() == ty) {
          return AtMost(arg, TaggedType::PtrInt());
        } else {
          return AtMost(arg, TaggedType::Int());
        }
      }
      case Type::F32:
      case Type::F64:
      case Type::F80:
      case Type::F128: {
        return AtMost(arg, TaggedType::Int());
      }
    }
    llvm_unreachable("invalid type");
  } else {
    return AtMost(arg, type);
  }
}
