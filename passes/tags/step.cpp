// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/step.h"
#include "passes/tags/register_analysis.h"

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
  Func *caller = call.getParent()->getParent();
  if (auto *f = call.GetDirectCallee()) {
    // Only evaluate if all args are known.
    llvm::SmallVector<TaggedType, 8> args;
    for (unsigned i = 0, n = call.arg_size(); i < n; ++i) {
      auto arg = analysis_.Find(call.arg(i));
      if (arg.IsUnknown()) {
        return;
      }
      if (IsCamlCall(f->GetCallingConv())) {
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
    if (kind_ == Kind::REFINE && !f->IsRoot() && !f->HasAddressTaken()) {
      for (auto *user : f->users()) {
        auto mov = ::cast_or_null<MovInst>(user);
        if (!mov) {
          continue;
        }
        auto movRef = mov->GetSubValue(0);
        for (auto *movUser : mov->users()) {
          auto otherCall = ::cast_or_null<CallSite>(movUser);
          if (!otherCall || otherCall == &call || otherCall->GetCallee() != movRef) {
            continue;
          }
          for (unsigned i = 0, n = otherCall->arg_size(); i < n; ++i) {
            if (args.size() <= i) {
              args.resize(i + 1, TaggedType::Undef());
            }
            args[i] |= analysis_.Find(otherCall->arg(i));
          }
        }
      }

      for (unsigned i = 0, n = f->params().size(); i < n; ++i) {
        for (auto *arg : analysis_.args_[{ f, i }]) {
          if (i < args.size()) {
            Mark(arg->GetSubValue(0), Clamp(args[i], arg->GetType()));
          } else {
            llvm_unreachable("not implemented");
          }
        }
      }
    } else {
      // Propagate values to arguments.
      for (unsigned i = 0, n = call.arg_size(); i < n; ++i) {
        auto type = args[i];
        for (auto *inst : analysis_.args_[std::make_pair(f, i)]) {
          auto ref = inst->GetSubValue(0);
          auto arg = Clamp(analysis_.Find(ref) | type, inst->GetType());
          Mark(ref, arg);
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
        Return(caller, tcall, values);
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
        if (auto *tcall = ::cast_or_null<const TailCallInst>(&call)) {
          std::vector<TaggedType> values(call.type_size(), TaggedType::PtrInt());
          Return(caller, tcall, values);
        } else {
          for (unsigned i = 0, n = call.GetNumRets(); i < n; ++i) {
            Mark(call.GetSubValue(i), TaggedType::PtrInt());
          }
        }
        return;
      }
      case CallingConv::CAML: {
        if (target_) {
          if (auto *tcall = ::cast_or_null<const TailCallInst>(&call)) {
            std::vector<TaggedType> values;
            values.push_back(TaggedType::Ptr());
            values.push_back(TaggedType::Young());
            for (unsigned i = 2, n = call.type_size(); i < n; ++i) {
              values.push_back(Infer(call.type(i)));
            }
            Return(caller, tcall, values);
          } else {
            Mark(call.GetSubValue(0), TaggedType::Ptr());
            Mark(call.GetSubValue(1), TaggedType::Young());
            for (unsigned i = 2, n = call.GetNumRets(); i < n; ++i) {
              auto ref = call.GetSubValue(i);
              Mark(ref, Infer(ref.GetType()));
            }
          }
        } else {
          llvm_unreachable("not implemented");
        }
        return;
      }
      case CallingConv::CAML_ALLOC: {
        if (target_) {
          Mark(call.GetSubValue(0), TaggedType::Ptr());
          Mark(call.GetSubValue(1), TaggedType::Young());
        } else {
          llvm_unreachable("not implemented");
        }
        return;
      }
      case CallingConv::CAML_GC: {
        if (target_) {
          Mark(call.GetSubValue(0), TaggedType::Ptr());
          Mark(call.GetSubValue(1), TaggedType::Young());
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
  
  if (vl.IsZero() || vr.IsZero()) {
    Mark(i, TaggedType::Zero());
    return;
  }
  if (vl.IsOne() || vr.IsOne()) {
    auto other = vl.IsOne() ? vr : vl;
    switch (other.GetKind()) {
      case TaggedType::Kind::UNKNOWN:
      case TaggedType::Kind::EVEN:
      case TaggedType::Kind::ODD:
      case TaggedType::Kind::INT:
      case TaggedType::Kind::ZERO:
      case TaggedType::Kind::ONE:
      case TaggedType::Kind::ZERO_ONE: {
        Mark(i, other);
        return;
      }
      case TaggedType::Kind::YOUNG:
      case TaggedType::Kind::HEAP:
      case TaggedType::Kind::VAL:
      case TaggedType::Kind::PTR:
      case TaggedType::Kind::PTR_INT:
      case TaggedType::Kind::PTR_NULL: {
        Mark(i, TaggedType::Int());
        return;
      }
      case TaggedType::Kind::UNDEF: {
        Mark(i, TaggedType::Undef());
        return;
      }
    }
    llvm_unreachable("invalid kind");
  } 
  if (vl.IsEven() || vr.IsEven()) {
    Mark(i, TaggedType::Even());
    return;
  }
  if (vl.IsOdd() && vr.IsOdd()) {
    Mark(i, TaggedType::Odd());
    return;
  } 
  Mark(i, TaggedType::Int());
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
  TaggedType ty = TaggedType::Unknown();
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    ty |= analysis_.Find(phi.GetValue(i));
  }
  if (ty.IsUnknown()) {
    return;
  }
  Mark(phi, Clamp(ty, phi.GetType()));
}

// -----------------------------------------------------------------------------
void Step::VisitReturnInst(ReturnInst &r)
{
  auto cc = r.getParent()->getParent()->GetCallingConv();

  // Collect the values returned by this function.
  std::vector<TaggedType> values;
  for (unsigned i = 0, n = r.arg_size(); i < n; ++i) {
    auto ret = analysis_.Find(r.arg(i));
    if (ret.IsUnknown()) {
      return;
    }
    switch (cc) {
      case CallingConv::SETJMP:
      case CallingConv::XEN:
      case CallingConv::INTR:
      case CallingConv::MULTIBOOT:
      case CallingConv::WIN64:
      case CallingConv::C: {
      	values.push_back(ret);
        continue;
      }
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC: 
      case CallingConv::CAML_GC: {
        switch (i) {
          case 0: values.push_back(TaggedType::Ptr()); continue;
          case 1: values.push_back(TaggedType::Young()); continue;
          default: values.push_back(ret); continue;
        }
        llvm_unreachable("invalid index");
      }
    }
    llvm_unreachable("unknown calling convention");
  }
  return Return(r.getParent()->getParent(), &r, values);
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
void Step::Return(
    Func *from,
    const Inst *inst,
    const std::vector<TaggedType> &values)
{
  // Aggregate the values with those that might be returned on other paths.
  // Propagate information to the callers of the function and chain tail calls.
  std::queue<std::tuple<Func *, const Inst *, std::vector<TaggedType>>> q;
  q.emplace(from, inst, values);
  while (!q.empty()) {
    auto [f, inst, rets] = q.front();
    q.pop();

    bool changed = false;
    switch (kind_) {
      case Kind::FORWARD: {
        auto &aggregate = analysis_.rets_.emplace(
            f,
            std::vector<TaggedType>{}
        ).first->second;

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
        break;
      }
      case Kind::REFINE: {
        for (Block &block : *f) {
          auto *term = block.GetTerminator();
          if (term == inst) {
            continue;
          }

          if (auto *ret = ::cast_or_null<ReturnInst>(term)) {
            unsigned n = ret->arg_size();
            rets.resize(n, TaggedType::Unknown());
            for (unsigned i = 0; i < n; ++i) {
              rets[i] |= analysis_.Find(ret->arg(i));
            }
            continue;
          }
          if (auto *tcall = ::cast_or_null<TailCallInst>(term)) {
            unsigned n = tcall->type_size();
            rets.resize(n, TaggedType::Unknown());
            if (auto *f = tcall->GetDirectCallee()) {
              auto it = analysis_.rets_.find(f);
              if (it != analysis_.rets_.end()) {
                for (unsigned i = 0; i < n; ++i) {
                  if (i < it->second.size()) {
                    rets[i] |= it->second[i];
                  } else {
                    rets[i] |= TaggedType::Undef();
                  }
                }
              } else {
                // No values to merge from this path.
              }
            } else {
              for (unsigned i = 0; i < n; ++i) {
                switch (tcall->GetCallingConv()) {
                  case CallingConv::SETJMP:
                  case CallingConv::XEN:
                  case CallingConv::INTR:
                  case CallingConv::MULTIBOOT:
                  case CallingConv::WIN64:
                  case CallingConv::C: {
                    rets[i] |= Infer(tcall->type(i));
                    continue;
                  }
                  case CallingConv::CAML: {
                    switch (i) {
                      case 0: {
                        rets[i] |= TaggedType::Ptr();
                        continue;
                      }
                      case 1: {
                        rets[i] |= TaggedType::Young();
                        continue;
                      }
                      default: {
                        rets[i] |= Infer(tcall->type(i));
                        continue;
                      }
                    }
                    llvm_unreachable("invalid index");
                  }
                  case CallingConv::CAML_ALLOC: {
                    llvm_unreachable("not implemented");
                  }
                  case CallingConv::CAML_GC: {
                    llvm_unreachable("not implemented");
                  }
                }
                llvm_unreachable("invalid calling convention");
              }
            }
            continue;
          }
          assert(!term->IsReturn() && "unknown return instruction");
        }
        analysis_.rets_.erase(f);
        analysis_.rets_.emplace(f, rets);
        break;
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
          if (!call || call->GetCallee() != mov->GetSubValue(0)) {
            continue;
          }

          if (auto *tcall = ::cast_or_null<TailCallInst>(call)) {
            q.emplace(tcall->getParent()->getParent(), tcall, rets);
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

// -----------------------------------------------------------------------------
TaggedType Step::Infer(Type ty)
{
  switch (ty) {
    case Type::V64: {
      return TaggedType::Val();
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      if (target_->GetPointerType() == ty) {
        return TaggedType::PtrInt();
      } else {
        return TaggedType::Int();
      }
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      return TaggedType::Int();
    }
  }
  llvm_unreachable("invalid type");
}
