// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/raw_ostream.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/printer.h"
#include "core/target.h"
#include "core/pass_manager.h"
#include "core/analysis/dominator.h"
#include "passes/verifier.h"



// -----------------------------------------------------------------------------
const char *VerifierPass::kPassID = "verifier";

// -----------------------------------------------------------------------------
VerifierPass::VerifierPass(PassManager *passManager)
  : Pass(passManager)
  , ptrTy_(GetTarget()->GetPointerType())
{
}

// -----------------------------------------------------------------------------
bool VerifierPass::Run(Prog &prog)
{
  if (GetConfig().Verify) {
    for (Func &func : prog) {
      Verify(func);
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
const char *VerifierPass::GetPassName() const
{
  return "Verifier";
}

// -----------------------------------------------------------------------------
void VerifierPass::Verify(Func &func)
{
  // ensure definitions dominate uses.
  DominatorTree DT(func);
  std::function<void(Block &block, std::set<Inst *> &)> check =
    [&] (Block &block, std::set<Inst *> &insts)
    {
      for (Inst &inst : block) {
        if (!inst.Is(Inst::Kind::PHI)) {
          for (auto value : inst.operand_values()) {
            if (auto op = ::cast_or_null<Inst>(value)) {
              if (!insts.count(op.Get())) {
                Error(inst, "def does not dominate use");
              }
            }
          }
        }
        insts.insert(&inst);
      }
      for (Block *succ : block.successors()) {
        for (auto &phi : succ->phis()) {
          if (!phi.HasValue(&block)) {
            std::string str;
            llvm::raw_string_ostream os(str);
            os << "missing PHI block " << block.getName() << " in " << succ->getName();
            Error(phi, os.str());
          } else {
            auto v = phi.GetValue(&block);
            if (!insts.count(v.Get())) {
              std::string str;
              llvm::raw_string_ostream os(str);
              os << "def does not dominate use in PHI from " << block.getName();
              Error(phi, os.str());
            }
          }
        }
      }
      for (const auto *child : *DT[&block]) {
        check(*child->getBlock(), insts);
      }
      for (Inst &inst : block) {
        insts.erase(&inst);
      }
    };
  std::set<Inst *> insts;
  check(func.getEntryBlock(), insts);

  // Verify properties of instructions.
  for (Block &block : func) {
    if (!block.GetTerminator()) {
      Error(block, "invalid terminator");
    }
    std::set<Block *> preds(block.pred_begin(), block.pred_end());
    for (PhiInst &phi : block.phis()) {
      std::set<Block *> ins;
      for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
        ins.insert(phi.GetBlock(i));
      }
      if (preds != ins) {
        std::string msg;
        llvm::raw_string_ostream os(msg);
        os << "invalid PHI predecessors: ";
        for (auto *pred : preds) {
          if (!ins.count(pred)) {
            os << " missing " << pred->getName();
          }
        }
        for (auto *in : ins) {
          if (!preds.count(in)) {
            os << " additional " << in->getName();
          }
        }
        Error(phi, msg);
      }
    }
    for (Inst &inst : block) {
      Dispatch(inst);
    }
  }
}

// -----------------------------------------------------------------------------
static bool Compatible(Type vt, Type type)
{
  if (vt == type) {
    return true;
  }
  if (type == Type::V64 || type == Type::I64) {
    return vt == Type::I64 || vt == Type::V64;
  }
  return false;
}

// -----------------------------------------------------------------------------
void VerifierPass::CheckPointer(
    const Inst &i,
    ConstRef<Inst> ref,
    const char *msg)
{
  if (!Compatible(ref.GetType(), ptrTy_)) {
    Error(i, msg);
  }
};

// -----------------------------------------------------------------------------
void VerifierPass::CheckInteger(
    const Inst &i,
    ConstRef<Inst> ref,
    const char *msg)
{
  if (!IsIntegerType(ref.GetType())) {
    Error(i, msg);
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::CheckType(const Inst &i, ConstRef<Inst> ref, Type type)
{
  auto it = ref.GetType();
  if (type == Type::I64 && it == Type::V64) {
    return;
  }
  if (type == Type::V64 && it == Type::I64) {
    return;
  }
  if (it != type) {
    Error(i, "invalid type");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::Error(const Block &block, llvm::Twine msg)
{
  const Func *func = block.getParent();
  std::string buffer;
  llvm::raw_string_ostream os(buffer);
  Printer p(os);
  os << "[" << func->getName() << ":" << block.getName() << "] " << msg.str();
  os << "\n\n";
  p.Print(*func);
  p.Print(block);
  os << "\n";
  llvm::report_fatal_error(os.str().c_str());
}

// -----------------------------------------------------------------------------
void VerifierPass::Error(const Inst &i, llvm::Twine msg)
{
  const Block *block = i.getParent();
  const Func *func = block->getParent();
  std::string buffer;
  llvm::raw_string_ostream os(buffer);
  Printer p(os);
  os << "[" << func->getName() << ":" << block->getName() << "] " << msg.str();
  os << "\n\n";
  p.Print(*i.getParent()->getParent());
  p.Print(i);
  os << "\n";
  llvm::report_fatal_error(os.str().c_str());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitConstInst(const ConstInst &i)
{

}

// -----------------------------------------------------------------------------
void VerifierPass::VisitUnaryInst(const UnaryInst &i)
{
  // Argument must match type.
  if (i.GetArg().GetType() != i.GetType()) {
    Error(i, "invalid argument type");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitBinaryInst(const BinaryInst &i)
{

}

// -----------------------------------------------------------------------------
void VerifierPass::VisitOverflowInst(const OverflowInst &i)
{
  // LHS must match RHS, return type integral.
  Type type = i.GetType();
  if (!IsIntegerType(type)) {
    Error(i, "integral type expected");
  }
  if (i.GetLHS().GetType() != i.GetRHS().GetType()) {
    Error(i, "invalid argument types");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitShiftRotateInst(const ShiftRotateInst &i)
{
  Type type = i.GetType();
  if (!IsIntegerType(type)) {
    Error(i, "integral type expected");
  }
  CheckType(i, i.GetLHS(), type);
  if (!IsIntegerType(i.GetRHS().GetType())) {
    Error(i, "integral type expected");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitMemoryInst(const MemoryInst &i)
{

}

// -----------------------------------------------------------------------------
void VerifierPass::VisitBarrierInst(const BarrierInst &i)
{

}

// -----------------------------------------------------------------------------
void VerifierPass::VisitMemoryExchangeInst(const MemoryExchangeInst &i)
{
  CheckPointer(i, i.GetAddr());
  if (i.GetValue().GetType() != i.GetType()) {
    Error(i, "invalid exchange");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitMemoryCompareExchangeInst(
    const MemoryCompareExchangeInst &i)
{
  CheckPointer(i, i.GetAddr());
  if (i.GetValue().GetType() != i.GetType()) {
    Error(i, "invalid exchange");
  }
  if (i.GetRef().GetType() != i.GetType()) {
    Error(i, "invalid exchange");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitLoadLinkInst(const LoadLinkInst &i)
{
  CheckPointer(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitStoreCondInst(const StoreCondInst &i)
{
  CheckPointer(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitCallSite(const CallSite &i)
{
  CheckPointer(i, i.GetCallee());
  // TODO: check arguments for direct callees.
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitX86_FPUControlInst(const X86_FPUControlInst &i)
{
  CheckPointer(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitPhiInst(const PhiInst &phi)
{
  for (Block *predBB : phi.getParent()->predecessors()) {
    if (!phi.HasValue(predBB)) {
      Error(phi, "missing predecessor to phi: " + predBB->getName());
    }
  }
  Type type = phi.GetType();
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    ConstRef<Value> value = phi.GetValue(i);
    switch (value->GetKind()) {
      case Value::Kind::INST: {
        auto vt = cast<Inst>(value).GetType();
        if (!Compatible(vt, type)) {
          Error(phi, "phi instruction argument invalid");
        }
        continue;
      }
      case Value::Kind::GLOBAL: {
        CheckPointer(phi, &phi, "phi must be of pointer type");
        continue;
      }
      case Value::Kind::EXPR: {
        const Expr &e = *cast<Expr>(value);
        switch (e.GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            CheckPointer(phi, &phi, "phi must be of pointer type");
            continue;
          }
        }
        llvm_unreachable("invalid expression kind");
      }
      case Value::Kind::CONST: {
        const Constant &c = *cast<Constant>(value);
        switch (c.GetKind()) {
          case Constant::Kind::INT: {
            continue;
          }
          case Constant::Kind::FLOAT: {
            return;
          }
        }
        llvm_unreachable("invalid constant kind");
      }
    }
    llvm_unreachable("invalid value kind");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitMovInst(const MovInst &mi)
{
  ConstRef<Value> value = mi.GetArg();
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      auto inst = ::cast<Inst>(value);
      if (!Compatible(mi.GetType(), inst.GetType())) {
        Error(mi, "incompatible move type");
      }
      return;
    }
    case Value::Kind::GLOBAL: {
      CheckPointer(mi, &mi, "global move not pointer sized");
      return;
    }
    case Value::Kind::EXPR: {
      const Expr &e = *cast<Expr>(value);
      switch (e.GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          CheckPointer(mi, &mi, "expression must be a pointer");
          return;
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::CONST: {
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitAllocaInst(const AllocaInst &i)
{
  if (i.GetType(0) != ptrTy_) {
    Error(i, "pointer type expected");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitFrameInst(const FrameInst &i)
{
  if (i.GetType(0) != ptrTy_) {
    Error(i, "pointer type expected");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitSetInst(const SetInst &i)
{
  switch (i.GetReg()) {
    case Register::X86_CS:
    case Register::X86_DS:
    case Register::X86_ES:
    case Register::X86_SS:
    case Register::X86_FS:
    case Register::X86_GS: {
      CheckType(i, i.GetValue(), Type::I32);
      return;
    }
    case Register::FS: {
      // TODO: ban writes to FS on X86.
      CheckPointer(i, i.GetValue(), "set expects a pointer");
      return;
    }
    case Register::SP:
    case Register::X86_CR0:
    case Register::X86_CR2:
    case Register::X86_CR3:
    case Register::X86_CR4:
    case Register::AARCH64_FPSR:
    case Register::AARCH64_FPCR:
    case Register::AARCH64_CNTVCT:
    case Register::AARCH64_CNTFRQ:
    case Register::AARCH64_FAR:
    case Register::AARCH64_VBAR:
    case Register::RISCV_FFLAGS:
    case Register::RISCV_FRM:
    case Register::RISCV_FCSR: {
      CheckPointer(i, i.GetValue(), "set expects a pointer");
      return;
    }
    case Register::PPC_FPSCR: {
      CheckType(i, i.GetValue(), Type::F64);
      return;
    }
    case Register::RET_ADDR:
    case Register::FRAME_ADDR: {
      Error(i, "Cannot overwrite immutable register");
      return;
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitGetInst(const GetInst &get)
{
  switch (get.GetReg()) {
    case Register::X86_CS:
    case Register::X86_DS:
    case Register::X86_ES:
    case Register::X86_SS:
    case Register::X86_FS:
    case Register::X86_GS: {
      CheckType(get, &get, Type::I32);
      return;
    }
    case Register::SP:
    case Register::FS:
    case Register::X86_CR0:
    case Register::X86_CR2:
    case Register::X86_CR3:
    case Register::X86_CR4:
    case Register::RET_ADDR:
    case Register::FRAME_ADDR:
    case Register::AARCH64_FPSR:
    case Register::AARCH64_FPCR:
    case Register::AARCH64_CNTVCT:
    case Register::AARCH64_CNTFRQ:
    case Register::AARCH64_FAR:
    case Register::AARCH64_VBAR:
    case Register::RISCV_FFLAGS:
    case Register::RISCV_FRM:
    case Register::RISCV_FCSR:  {
      CheckPointer(get, &get, "registers return pointers");
      return;
    }
    case Register::PPC_FPSCR: {
      CheckType(get, &get, Type::F64);
      return;
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitCmpInst(const CmpInst &i)
{
  auto lt = i.GetLHS().GetType();
  auto rt = i.GetRHS().GetType();
  bool lptr = lt == Type::V64 && rt == Type::I64;
  bool rptr = rt == Type::V64 && lt == Type::I64;
  if (lt != rt && !lptr && !rptr) {
    Error(i, "invalid arguments to comparison");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitSyscallInst(const SyscallInst &i)
{
  for (auto arg : i.args()) {
    CheckInteger(i, arg, "syscall expects integer arguments");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitArgInst(const ArgInst &i)
{
  unsigned idx = i.GetIndex();
  const auto &params = i.getParent()->getParent()->params();
  if (idx >= params.size()) {
    Error(i, "argument out of range");
  }
  if (params[idx].GetType() != i.GetType()) {
    Error(i, "argument type mismatch");
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitRaiseInst(const RaiseInst &i)
{
  CheckPointer(i, i.GetTarget());
  CheckPointer(i, i.GetStack());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitLandingPadInst(const LandingPadInst &i)
{
  const Block *block = i.getParent();
  if (&i != &*block->begin()) {
    auto prev = std::prev(i.getIterator());
    if (!prev->Is(Inst::Kind::PHI)) {
      Error(i, "landing pad is not the first instruction");
    }
  }
  for (const Block *pred : block->predecessors()) {
    if (!pred->GetTerminator()->Is(Inst::Kind::INVOKE)) {
      Error(i, "landing pad not reached through an invoke");
    }
  }
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitLoadInst(const LoadInst &i)
{
  CheckPointer(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitStoreInst(const StoreInst &i)
{
  CheckPointer(i, i.GetAddr());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitVaStartInst(const VaStartInst &i)
{
  CheckPointer(i, i.GetVAList());
}

// -----------------------------------------------------------------------------
void VerifierPass::VisitSelectInst(const SelectInst &i)
{
  if (!Compatible(i.GetTrue().GetType(), i.GetType())) {
    Error(i, "mismatched true branch");
  }
  if (!Compatible(i.GetFalse().GetType(), i.GetType())) {
    Error(i, "mismatched false branches");
  }
}

