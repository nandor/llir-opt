// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType Step::Clamp(TaggedType type, Type ty)
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
void Step::VisitCallSite(CallSite &call)
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
        auto arg = Clamp(analysis_.Find(ref) | type, inst->GetType());
        Mark(ref, arg);
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
            Mark(call.GetSubValue(i), Clamp(it->second[i], call.type(i)));
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
          Mark(call.GetSubValue(i), TaggedType::Val());
        }
        return;
      }
      case CallingConv::CAML: {
        if (target_) {
          Mark(call.GetSubValue(0), TaggedType::Ptr());
          Mark(call.GetSubValue(1), TaggedType::Young());
          for (unsigned i = 2, n = call.GetNumRets(); i < n; ++i) {
            auto ref = call.GetSubValue(i);
            switch (auto ty = call.type(i)) {
              case Type::V64: {
                Mark(ref, TaggedType::Val());
                continue;
              }
              case Type::I8:
              case Type::I16:
              case Type::I32:
              case Type::I64:
              case Type::I128: {
                if (target_->GetPointerType() == ty) {
                  Mark(ref, TaggedType::PtrInt());
                } else {
                  Mark(ref, TaggedType::Int());
                }
                continue;
              }
              case Type::F32:
              case Type::F64:
              case Type::F80:
              case Type::F128: {
                Mark(ref, TaggedType::Int());
                continue;
              }
            }
            llvm_unreachable("invalid type");
          }
        } else llvm_unreachable("not implemented");
        return;
      }
      case CallingConv::CAML_ALLOC: {
        llvm_unreachable("not implemented");
      }
      case CallingConv::CAML_GC: {
        if (target_) {
          Mark(call.GetSubValue(0), TaggedType::Ptr());
          Mark(call.GetSubValue(1), TaggedType::Young());
        } else llvm_unreachable("not implemented");
        return;
      }
    }
    llvm_unreachable("unknown calling convention");
  }
}

// -----------------------------------------------------------------------------
void Step::VisitMovInst(MovInst &i)
{
  if (auto inst = ::cast_or_null<Inst>(i.GetArg())) {
    auto val = analysis_.Find(inst);
    assert(!val.IsUnknown() && "cannot propagate unknown");
    Mark(i, Clamp(val, i.GetType()));
  }
}

// -----------------------------------------------------------------------------
void Step::VisitAddInst(AddInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = Add(vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, Clamp(r, i.GetType()));
  }
}

// -----------------------------------------------------------------------------
void Step::VisitSubInst(SubInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = Sub(i.GetType(), vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, r);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitMultiplyInst(MultiplyInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }

  if (vl.IsEven() || vr.IsEven()) {
    Mark(i, TaggedType::Even());
  } else if (vl.IsOdd() && vr.IsOdd()) {
    Mark(i, TaggedType::Odd());
  } else {
    Mark(i, TaggedType::Int());
  }
}

// -----------------------------------------------------------------------------
void Step::VisitDivisionRemainderInst(DivisionRemainderInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }
  Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Step::VisitAndInst(AndInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = And(i.GetType(), vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, r);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitXorInst(XorInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = Xor(i.GetType(), vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, r);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitOrInst(OrInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = Or(i.GetType(), vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, r);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitShiftRightInst(ShiftRightInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = Shr(i.GetType(), vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, r);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitSllInst(SllInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  auto r = Shl(i.GetType(), vl, vr);
  if (!r.IsUnknown()) {
    Mark(i, r);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitRotlInst(RotlInst &i)
{
  auto vl = analysis_.Find(i.GetLHS());
  auto vr = analysis_.Find(i.GetRHS());
  if (vl.IsUnknown() || vr.IsUnknown()) {
    return;
  }
  Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Step::VisitExtensionInst(ExtensionInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  auto ret = Ext(i.GetType(), arg);
  if (!ret.IsUnknown()) {
    Mark(i, ret);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitTruncInst(TruncInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  auto ret = Trunc(i.GetType(), arg);
  if (!ret.IsUnknown()) {
    Mark(i, ret);
  }
}

// -----------------------------------------------------------------------------
void Step::VisitBitCastInst(BitCastInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  if (arg.IsUnknown()) {
    return;
  }
  Mark(i, arg);
}

// -----------------------------------------------------------------------------
void Step::VisitByteSwapInst(ByteSwapInst &i)
{
  auto arg = analysis_.Find(i.GetArg());
  if (arg.IsUnknown()) {
    return;
  }
  Mark(i, TaggedType::Int());
}

// -----------------------------------------------------------------------------
void Step::VisitMemoryExchangeInst(MemoryExchangeInst &i)
{
  Mark(i, Clamp(TaggedType::PtrInt(), i.GetType()));
}

// -----------------------------------------------------------------------------
void Step::VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i)
{
  auto addr = analysis_.Find(i.GetAddr());
  auto val = analysis_.Find(i.GetValue());
  auto ref = analysis_.Find(i.GetRef());
  if (ref.IsUnknown() || val.IsUnknown() || val.IsUnknown()) {
    return;
  }
  Mark(i, Clamp(TaggedType::PtrInt(), i.GetType()));
}

// -----------------------------------------------------------------------------
void Step::VisitSelectInst(SelectInst &select)
{
  auto vt = analysis_.Find(select.GetTrue());
  auto vf = analysis_.Find(select.GetFalse());
  if (vt.IsUnknown() || vf.IsUnknown()) {
    return;
  }
  Mark(select, vt | vf);
}

// -----------------------------------------------------------------------------
void Step::VisitPhiInst(PhiInst &phi)
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
    Mark(phi, Clamp(*type, phi.GetType()));
  }
}

// -----------------------------------------------------------------------------
void Step::VisitReturnInst(ReturnInst &r)
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
bool Step::Mark(Ref<Inst> inst, const TaggedType &type)
{
  switch (kind_) {
    case Kind::REFINE: {
      return analysis_.Refine(inst, type);
    }
    case Kind::FORWARD: {
      return analysis_.Mark(inst, type);
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
void Step::Return(Func *from, const std::vector<TaggedType> &values)
{
  // Aggregate the values with those that might be returned on other paths.
  // Propagate information to the callers of the function and chain tail calls.
  std::queue<std::pair<Func *, std::vector<TaggedType>>> q;
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
        auto *mov = ::cast_or_null<MovInst>(user);
        if (!mov) {
          continue;
        }
        for (auto *movUser : mov->users()) {
          auto *call = ::cast_or_null<CallSite>(movUser);
          if (!call) {
            continue;
          }

          if (auto *tcall = ::cast_or_null<TailCallInst>(call)) {
            q.emplace(tcall->getParent()->getParent(), rets);
          } else {
            for (unsigned i = 0, n = call->GetNumRets(); i < n; ++i) {
              if (i < rets.size()) {
                Mark(call->GetSubValue(i), Clamp(rets[i], call->type(i)));
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

// -----------------------------------------------------------------------------
void Step::VisitInst(Inst &i)
{
  std::string msg;
  llvm::raw_string_ostream os(msg);
  os << i << "\n";
  llvm::report_fatal_error(msg.c_str());
}
