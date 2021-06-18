// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/tags/constraints.h"
#include "passes/tags/register_analysis.h"

using namespace tags;


// -----------------------------------------------------------------------------
bool ConstraintSolver::IsExtern(const Func &f)
{
  auto it = externs_.emplace(&f, false);
  if (it.second) {
    it.first->second = f.IsRoot() || f.HasAddressTaken();
  }
  return it.first->second;
}


// -----------------------------------------------------------------------------
void ConstraintSolver::VisitArgInst(ArgInst &arg)
{
  auto idx = arg.GetIndex();
  auto &func = *arg.getParent()->getParent();
  for (auto *user : func.users()) {
    auto mov = ::cast_or_null<MovInst>(user);
    if (!mov) {
      continue;
    }
    for (auto *movUser : mov->users()) {
      auto call = ::cast_or_null<CallSite>(movUser);
      if (!call || call->GetCallee() != mov->GetSubValue(0)) {
        continue;
      }
      if (idx < call->arg_size()) {
        switch (func.GetCallingConv()) {
          case CallingConv::C:
          case CallingConv::SETJMP:
          case CallingConv::XEN:
          case CallingConv::INTR:
          case CallingConv::MULTIBOOT:
          case CallingConv::WIN64: {
            if (IsExtern(func)) {
              Infer(arg);
            } else {
              Subset(call->arg(idx), arg);
            }
            continue;
          }
          case CallingConv::CAML: {
            if (target_) {
              switch (target_->GetKind()) {
                case Target::Kind::X86: {
                  switch (idx) {
                    case 0: ExactlyPointer(arg); continue;
                    case 1: ExactlyYoung(arg); continue;
                    default: {
                      if (IsExtern(func)) {
                        Infer(arg);
                      } else {
                        Subset(call->arg(idx), arg);
                      }
                      continue;
                    }
                  }
                  llvm_unreachable("invalid argument index");
                }
                case Target::Kind::PPC: {
                  llvm_unreachable("not implemented");
                }
                case Target::Kind::AARCH64: {
                  llvm_unreachable("not implemented");
                }
                case Target::Kind::RISCV: {
                  llvm_unreachable("not implemented");
                }
              }
              llvm_unreachable("invalid target kind");
            } else {
              llvm_unreachable("not implemented");
            }
          }
          case CallingConv::CAML_ALLOC: {
            if (target_) {
              switch (target_->GetKind()) {
                case Target::Kind::X86: {
                  switch (idx) {
                    case 0: ExactlyPointer(arg); continue;
                    case 1: ExactlyYoung(arg); continue;
                    default: llvm_unreachable("invalid argument");
                  }
                  llvm_unreachable("invalid argument index");
                }
                case Target::Kind::PPC: {
                  llvm_unreachable("not implemented");
                }
                case Target::Kind::AARCH64: {
                  llvm_unreachable("not implemented");
                }
                case Target::Kind::RISCV: {
                  llvm_unreachable("not implemented");
                }
              }
              llvm_unreachable("invalid target kind");
            } else {
              llvm_unreachable("not implemented");
            }
          }
          case CallingConv::CAML_GC: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid calling convention");
      } else {
        Infer(arg);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitCallSite(CallSite &site)
{
  // Enforce a function pointer constraint on the callee.
  ExactlyFunc(site.GetCallee());

  // Add subset constraints on returns entering here.
  std::queue<Func *> q;
  llvm::SmallPtrSet<Func *, 8> v;
  llvm::SmallPtrSet<ReturnInst *, 8> subsets;
  bool clamp = false;

  if (auto *f = site.GetDirectCallee()) {
    q.push(f);
    while (!q.empty()) {
      auto *f = q.front();
      q.pop();
      if (!v.insert(f).second) {
        continue;
      }

      for (Block &block : *f) {
        auto *term = block.GetTerminator();
        if (!term->IsReturn()) {
          continue;
        }
        if (auto *ret = ::cast_or_null<ReturnInst>(term)) {
          subsets.insert(ret);
          for (unsigned i = 0, n = site.GetNumRets(); i < n; ++i) {
            if (i < ret->arg_size()) {
              auto arg = ret->arg(i);
              auto dst = site.GetSubValue(i);
              if (!(analysis_.Find(arg) <= analysis_.Find(dst))) {
                clamp = true;
              }
            } else {
              llvm_unreachable("not implemented");
            }
          }
          continue;
        }
        if (auto *tcall = ::cast_or_null<TailCallInst>(term)) {
          if (auto *f = tcall->GetDirectCallee()) {
            q.push(f);
          } else {
            clamp = true;
          }
          continue;
        }
        llvm_unreachable("invalid return instruction");
      }
    }
  }

  // Enfore the widest constraints on the return values.
  if (clamp) {
    switch (site.GetCallingConv()) {
      case CallingConv::SETJMP:
      case CallingConv::XEN:
      case CallingConv::INTR:
      case CallingConv::MULTIBOOT:
      case CallingConv::WIN64:
      case CallingConv::C: {
        for (unsigned i = 0, n = site.GetNumRets(); i < n; ++i) {
          Infer(site.GetSubValue(i));
        }
        break;
      }
      case CallingConv::CAML: {
        ExactlyPointer(site.GetSubValue(0));
        ExactlyYoung(site.GetSubValue(1));
        for (unsigned i = 2, n = site.GetNumRets(); i < n; ++i) {
          Infer(site.GetSubValue(i));
        }
        break;
      }
      case CallingConv::CAML_ALLOC: {
        ExactlyPointer(site.GetSubValue(0));
        ExactlyYoung(site.GetSubValue(1));
        break;
      }
      case CallingConv::CAML_GC: {
        ExactlyPointer(site.GetSubValue(0));
        ExactlyYoung(site.GetSubValue(1));
        break;
      }
    }
  } else {
    for (unsigned i = 0, n = site.GetNumRets(); i < n; ++i) {
      for (auto *ret : subsets) {
        if (i < ret->arg_size()) {
          Subset(ret->arg(i), site.GetSubValue(i));
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ConstraintSolver::VisitLandingPadInst(LandingPadInst &pad)
{
  if (target_) {
    switch (target_->GetKind()) {
      case Target::Kind::X86: {
        if (auto cc = pad.GetCallingConv()) {
          switch (*cc) {
            case CallingConv::CAML: {
              ExactlyPointer(pad.GetSubValue(0));
              ExactlyYoung(pad.GetSubValue(1));
              for (unsigned i = 2, n = pad.GetNumRets(); i < n; ++i) {
                Infer(pad.GetSubValue(i));
              }
              return;
            }
            case CallingConv::CAML_ALLOC:
            case CallingConv::CAML_GC:
            case CallingConv::SETJMP:
            case CallingConv::XEN:
            case CallingConv::INTR:
            case CallingConv::MULTIBOOT:
            case CallingConv::WIN64:
            case CallingConv::C: {
              llvm_unreachable("not implemented");
            }
          }
          llvm_unreachable("unknown calling convention");
        } else {
          llvm_unreachable("not implemented");
        }
      }
      case Target::Kind::PPC: {
        llvm_unreachable("not implemented");
      }
      case Target::Kind::AARCH64: {
        llvm_unreachable("not implemented");
      }
      case Target::Kind::RISCV: {
        llvm_unreachable("not implemented");
      }
    }
    llvm_unreachable("invalid target kind");
  } else {
    llvm_unreachable("not implemented");
  }
}
