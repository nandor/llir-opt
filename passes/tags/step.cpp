// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
static TaggedType Clamp(TaggedType type, Type ty)
{
  if (ty == Type::V64) {
    if (type <= TaggedType::Val()) {
      return type;
    } else {
      return TaggedType::Val();
    }
  } else {
    return type;
  }
}

// -----------------------------------------------------------------------------
void Step::VisitCallSite(const CallSite &call)
{
  const Func *caller = call.getParent()->getParent();
  if (auto *f = call.GetDirectCallee()) {
    // Only evaluate if all args are known.
    llvm::SmallVector<TaggedType, 8> args;
    for (unsigned i = 0, n = call.arg_size(); i < n; ++i) {
      auto arg = analysis_.Find(call.arg(i));
      if (arg.IsUnknown()) {
        return;
      }
      if (!IsCamlCall(caller->GetCallingConv()) && IsCamlCall(f->GetCallingConv())) {
        switch (i) {
          case 0: {
            args.push_back(TaggedType::Ptr());
            break;
          }
          case 1: {
            args.push_back(TaggedType::Young());
            break;
          }
          default: {
            args.push_back(arg);
            break;
          }
        }
      } else {
        args.push_back(arg);
      }
    }
    // Propagate values to arguments.
    for (unsigned i = 0, n = call.arg_size(); i < n; ++i) {
      auto type = args[i];
      for (auto *inst : analysis_.args_[std::make_pair(f, i)]) {
        auto ref = inst->GetSubValue(0);
        auto it = analysis_.types_.emplace(ref, type);
        if (it.second) {
          analysis_.Enqueue(ref);
        } else {
          if (it.first->second != type) {
            auto lub = it.first->second | type;
            if (lub != it.first->second) {
              it.first->second = lub;
              analysis_.Enqueue(ref);
            }
          }
        }
      }
    }
    // If the callee recorded a value already, propagate it.
    if (auto it = analysis_.rets_.find(f); it != analysis_.rets_.end()) {
      if (auto *tcall = ::cast_or_null<const TailCallInst>(&call)) {
        std::vector<TaggedType> values;
        for (unsigned i = 0, n = tcall->type_size(); i < n; ++i) {
          if (i < it->second.size()) {
            values.push_back(it->second[i]);
          } else {
            llvm_unreachable("not implemented");
          }
        }
        Return(tcall->getParent()->getParent(), values);
      } else {
        for (unsigned i = 0, n = call.GetNumRets(); i < n; ++i) {
          if (i < it->second.size()) {
            analysis_.Mark(call.GetSubValue(i), Clamp(it->second[i], call.type(i)));
          } else {
            llvm_unreachable("not implemented");
          }
        }
      }
    }
  } else {
    switch (call.GetCallingConv()) {
      case CallingConv::SETJMP:
      case CallingConv::XEN:
      case CallingConv::INTR:
      case CallingConv::MULTIBOOT:
      case CallingConv::WIN64:
      case CallingConv::C: {
        for (unsigned i = 0, n = call.GetNumRets(); i < n; ++i) {
          analysis_.Mark(call.GetSubValue(i), TaggedType::Val());
        }
        return;
      }
      case CallingConv::CAML: {
        if (target_) {
          analysis_.Mark(call.GetSubValue(0), TaggedType::Ptr());
          analysis_.Mark(call.GetSubValue(1), TaggedType::Young());
          for (unsigned i = 2, n = call.GetNumRets(); i < n; ++i) {
            auto ref = call.GetSubValue(i);
            switch (auto ty = call.type(i)) {
              case Type::V64: {
                analysis_.Mark(ref, TaggedType::Val());
                continue;
              }
              case Type::I8:
              case Type::I16:
              case Type::I32:
              case Type::I64:
              case Type::I128: {
                if (target_->GetPointerType() == ty) {
                  analysis_.Mark(ref, TaggedType::IntOrPtr());
                } else {
                  analysis_.Mark(ref, TaggedType::Int());
                }
                continue;
              }
              case Type::F32:
              case Type::F64:
              case Type::F80:
              case Type::F128: {
                analysis_.Mark(ref, TaggedType::Int());
                continue;
              }
            }
            llvm_unreachable("invalid type");
          }
        } else {
          llvm_unreachable("not implemented");
        }
        return;
      }
      case CallingConv::CAML_ALLOC: {
        llvm_unreachable("not implemented");
      }
      case CallingConv::CAML_GC: {
        if (target_) {
          analysis_.Mark(call.GetSubValue(0), TaggedType::Ptr());
          analysis_.Mark(call.GetSubValue(1), TaggedType::Young());
        } else {
          llvm_unreachable("not implemented");
        }
        return;
      }
    }
    llvm_unreachable("unknown calling convention");
  }
}

// -----------------------------------------------------------------------------
void Step::VisitMovInst(const MovInst &i)
{
  if (auto inst = ::cast_or_null<const Inst>(i.GetArg())) {
    auto val = analysis_.Find(inst);
    assert(!val.IsUnknown() && "cannot propagate unknown");
    analysis_.Mark(i, Clamp(val, i.GetType()));
  }
}

// -----------------------------------------------------------------------------
void Step::VisitAddInst(const AddInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, vr);
          return;
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::HEAP: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ODD: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, vr);
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::One());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, vr);
      }
      return;
    }
    case TaggedType::Kind::ZERO_OR_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, vl);
          return;
        }
        case TaggedType::Kind::VAL: {
          if (i.GetType() == Type::V64) {
            analysis_.Mark(i, TaggedType::Val());
          } else {
            analysis_.Mark(i, TaggedType::IntOrPtr());
          }
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, vr);
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::INT_OR_PTR: {
          if (i.GetType() == Type::V64) {
            analysis_.Mark(i, TaggedType::Val());
          } else {
            analysis_.Mark(i, TaggedType::IntOrPtr());
          }
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Heap());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: {
      if (i.GetType() == Type::V64) {
        analysis_.Mark(i, TaggedType::Heap());
      } else {
        analysis_.Mark(i, TaggedType::Young());
      }
      return;
    }
    case TaggedType::Kind::UNDEF: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, TaggedType::Undef());
      }
      return;
    }
    case TaggedType::Kind::INT_OR_PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, vl);
          return;
        }
        case TaggedType::Kind::VAL: {
          if (i.GetType() == Type::V64) {
            analysis_.Mark(i, TaggedType::Val());
          } else {
            analysis_.Mark(i, TaggedType::IntOrPtr());
          }
          return;
        }
        case TaggedType::Kind::HEAP: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ANY: {
      analysis_.Mark(i, vl);
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitSubInst(const SubInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ODD: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::ODD: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, TaggedType::Int());
      }
      return;
    }
    case TaggedType::Kind::ZERO_OR_ONE: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::INT: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          if (i.GetType() == Type::V64) {
            analysis_.Mark(i, TaggedType::Val());
          } else {
            analysis_.Mark(i, TaggedType::IntOrPtr());
          }
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT: 
        case TaggedType::Kind::ODD: 
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          analysis_.Mark(i, TaggedType::Undef());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, TaggedType::Young());
      }
      return;
    }
    case TaggedType::Kind::UNDEF: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::INT_OR_PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ANY: {
      analysis_.Mark(i, TaggedType::Any());
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitMultiplyInst(const MultiplyInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }

  if (vl.IsEven() || vr.IsEven()) {
    analysis_.Mark(i, TaggedType::Even());
  } else if (vl.IsOdd() && vr.IsOdd()) {
    analysis_.Mark(i, TaggedType::Odd());
  } else {
    analysis_.Mark(i, TaggedType::Int());
  }
}

// -----------------------------------------------------------------------------
void Step::VisitDivisionRemainderInst(const DivisionRemainderInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Step::VisitAndInst(const AndInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ODD: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i,TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i,TaggedType::One());
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i,TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::One());
          return;
        }
        case TaggedType::Kind::INT:
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::HEAP:
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::YOUNG:
        case TaggedType::Kind::UNDEF:
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, TaggedType::Zero());
      }
      return;
    }
    case TaggedType::Kind::ZERO_OR_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::HEAP:
        case TaggedType::Kind::PTR:
        case TaggedType::Kind::YOUNG:
        case TaggedType::Kind::UNDEF:
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ODD: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ODD: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::YOUNG: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::UNDEF: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::INT_OR_PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, vl);
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
        case TaggedType::Kind::VAL: {
          // TODO: could be more strict.
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          // TODO: this might be less generic than any.
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ANY: {
      // TODO: this might be less generic than any.
      analysis_.Mark(i, TaggedType::Any());
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitXorInst(const XorInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }
  analysis_.Mark(i, vl | vr);
}

// -----------------------------------------------------------------------------
void Step::VisitOrInst(const OrInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::EVEN: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ODD: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, vr);
      }
      return;
    }
    case TaggedType::Kind::ZERO_OR_ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ODD: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::VAL:
        case TaggedType::Kind::HEAP: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::PTR: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          analysis_.Mark(i, TaggedType::Undef());
          return;
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::INT: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          analysis_.Mark(i, TaggedType::IntOrPtr());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Odd());
          return;
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, TaggedType::Val());
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::Ptr());
          return;
        }
        case TaggedType::Kind::INT: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ODD: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::YOUNG: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::UNDEF: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::INT_OR_PTR: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, vl);
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::INT_OR_PTR:
        case TaggedType::Kind::ONE:
        case TaggedType::Kind::ZERO:
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::VAL: {
          analysis_.Mark(i, Clamp(TaggedType::IntOrPtr(), i.GetType()));
          return;
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          analysis_.Mark(i, TaggedType::Any());
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ANY: {
      analysis_.Mark(i, TaggedType::Any());
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitShiftRightInst(const ShiftRightInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  switch (vl.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::ONE: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::ZERO_OR_ONE:
        case TaggedType::Kind::INT:
        case TaggedType::Kind::EVEN: {
          analysis_.Mark(i, TaggedType::ZeroOrOne());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Zero());
          return;
        }
        case TaggedType::Kind::ZERO: {
          analysis_.Mark(i, TaggedType::One());
          return;
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::ZERO: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, TaggedType::Zero());
      }
      return;
    }
    case TaggedType::Kind::ZERO_OR_ONE:
    case TaggedType::Kind::INT: {
      if (!vr.IsUnknown()) {
        analysis_.Mark(i, TaggedType::Int());
      }
      return;
    }
    case TaggedType::Kind::VAL: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN:
        case TaggedType::Kind::INT: {
          // TODO: distinguish even from constant.
          analysis_.Mark(i, TaggedType::Even());
          return;
        }
        case TaggedType::Kind::ODD:
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::HEAP: {
      switch (vr.GetKind()) {
        case TaggedType::Kind::UNKNOWN: {
          return;
        }
        case TaggedType::Kind::EVEN: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::INT: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ODD: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ONE: {
          analysis_.Mark(i, TaggedType::Int());
          return;
        }
        case TaggedType::Kind::INT_OR_PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case TaggedType::Kind::ANY: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid value kind");
    }
    case TaggedType::Kind::PTR: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::YOUNG: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::UNDEF: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::ANY:
    case TaggedType::Kind::INT_OR_PTR: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitSllInst(const SllInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  if (vl.IsUnknown()) {
    return;
  }
  auto vr = analysis_.Find(i.GetRHS());
  switch (vr.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ODD: {
      analysis_.Mark(i, TaggedType::Even());
      return;
    }
    case TaggedType::Kind::INT:
    case TaggedType::Kind::INT_OR_PTR:
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::ZERO_OR_ONE: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::VAL: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::HEAP: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::PTR: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::YOUNG: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::UNDEF: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::ANY: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitRotlInst(const RotlInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Step::VisitExtensionInst(const ExtensionInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD:
    case TaggedType::Kind::INT: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ZERO_OR_ONE: {
      analysis_.Mark(i, arg);
      return;
    }
    case TaggedType::Kind::VAL:
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::YOUNG: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::PTR: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::INT_OR_PTR: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::UNDEF:
    case TaggedType::Kind::ANY: {
      analysis_.Mark(i, arg);
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitTruncInst(const TruncInst &i)
{
  auto arg = analysis_.Find(i.GetArg());

  // Determine whether the type can fit a pointer.
  bool fitsPointer;
  if (target_ && GetSize(i.GetType()) < GetSize(target_->GetPointerType())) {
    fitsPointer = false;
  } else {
    fitsPointer = true;
  }
  switch (arg.GetKind()) {
    case TaggedType::Kind::UNKNOWN: {
      return;
    }
    case TaggedType::Kind::ZERO:
    case TaggedType::Kind::EVEN:
    case TaggedType::Kind::ODD:
    case TaggedType::Kind::ONE:
    case TaggedType::Kind::ZERO_OR_ONE:
    case TaggedType::Kind::INT: {
      analysis_.Mark(i, arg);
      return;
    }
    case TaggedType::Kind::VAL: {
      analysis_.Mark(i, TaggedType::Int());
      return;
    }
    case TaggedType::Kind::UNDEF: {
      analysis_.Mark(i, TaggedType::Undef());
      return;
    }
    case TaggedType::Kind::HEAP:
    case TaggedType::Kind::PTR:
    case TaggedType::Kind::INT_OR_PTR: {
      if (fitsPointer) {
        analysis_.Mark(i, arg);
      } else {
        analysis_.Mark(i, TaggedType::Int());
      }
      return;
    }
    case TaggedType::Kind::YOUNG: {
      llvm_unreachable("not implemented");
    }
    case TaggedType::Kind::ANY: {
      if (fitsPointer) {
        analysis_.Mark(i, TaggedType::Any());
      } else {
        analysis_.Mark(i, TaggedType::Int());
      }
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Step::VisitBitCastInst(const BitCastInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  if (arg.IsUnknown()) {
    return;
  }
  analysis_.Mark(i, arg);
}

// -----------------------------------------------------------------------------
void Step::VisitByteSwapInst(const ByteSwapInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  if (arg.IsUnknown()) {
    return;
  }
  analysis_.Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Step::VisitMemoryCompareExchangeInst(const MemoryCompareExchangeInst &i)
{
  auto addr = analysis_.Find(i.GetAddr());
  auto val = analysis_.Find(i.GetValue());
  auto ref = analysis_.Find(i.GetRef());
  if (ref.IsUnknown() || val.IsUnknown() || val.IsUnknown()) {
    return;
  }
  analysis_.Mark(i, ref);
}

// -----------------------------------------------------------------------------
void Step::VisitSelectInst(const SelectInst &select)
{
  auto vt = analysis_.Find(select.GetTrue());
  auto vf = analysis_.Find(select.GetFalse());
  if (vt.IsUnknown() || vf.IsUnknown()) {
    return;
  }
  analysis_.Mark(select, vt | vf);
}

// -----------------------------------------------------------------------------
void Step::VisitPhiInst(const PhiInst &phi)
{
  std::optional<TaggedType> type;
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    auto val = phi.GetValue(i);
    if (type) {
      *type |= analysis_.Find(val);
    } else {
      type = analysis_.Find(val);
    }
  }
  if (type) {
    analysis_.Mark(phi, Clamp(*type, phi.GetType()));
  }
}

// -----------------------------------------------------------------------------
void Step::VisitReturnInst(const ReturnInst &r)
{
  // Collect the values returned by this function.
  std::vector<TaggedType> values;
  for (unsigned i = 0, n = r.arg_size(); i < n; ++i) {
    auto ret = analysis_.Find(r.arg(i));
    if (ret.IsUnknown()) {
      return;
    }
    values.push_back(ret);
  }
  return Return(r.getParent()->getParent(), values);
}

// -----------------------------------------------------------------------------
void Step::Return(const Func *from, const std::vector<TaggedType> &values)
{
  // Aggregate the values with those that might be returned on other paths.
  // Propagate information to the callers of the function and chain tail calls.
  std::queue<std::pair<const Func *, std::vector<TaggedType>>> q;
  q.emplace(from, values);
  while (!q.empty()) {
    auto [f, rets] = q.front();
    q.pop();

    auto &aggregate = analysis_.rets_.emplace(
        f,
        std::vector<TaggedType>{}
    ).first->second;

    bool changed = false;
    for (unsigned i = 0, n = rets.size(); i < n; ++i) {
      if (aggregate.size() <= i) {
        aggregate.resize(i, TaggedType::Undef());
        aggregate.push_back(rets[i]);
        changed = true;
      } else {
        const auto &ret = rets[i] |= aggregate[i];
        if (aggregate[i] != ret) {
          changed = true;
          aggregate[i] = ret;
        }
      }
    }

    if (changed) {
      for (auto *user : f->users()) {
        auto *mov = ::cast_or_null<const MovInst>(user);
        if (!mov) {
          continue;
        }
        for (auto *movUser : mov->users()) {
          auto *call = ::cast_or_null<const CallSite>(movUser);
          if (!call) {
            continue;
          }

          if (auto *tcall = ::cast_or_null<const TailCallInst>(call)) {
            q.emplace(tcall->getParent()->getParent(), rets);
          } else {
            for (unsigned i = 0, n = call->GetNumRets(); i < n; ++i) {
              if (i < rets.size()) {
                analysis_.Mark(call->GetSubValue(i), Clamp(rets[i], call->type(i)));
              } else {
                llvm_unreachable("not implemented");
              }
            }
          }
        }
      }
    }
  }
}
