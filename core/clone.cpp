// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
CloneVisitor::~CloneVisitor()
{
}

// -----------------------------------------------------------------------------
void CloneVisitor::Fixup()
{
  for (auto &phi : fixups_) {
    auto *phiOld = phi.first;
    auto *phiNew = phi.second;
    for (unsigned i = 0; i < phiOld->GetNumIncoming(); ++i) {
      phiNew->Add(Map(phiOld->GetBlock(i)), Map(phiOld->GetValue(i)));
    }
  }
  fixups_.clear();
}

// -----------------------------------------------------------------------------
Ref<Value> CloneVisitor::Map(Ref<Value> value)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: return Map(cast<Inst>(value));
    case Value::Kind::GLOBAL: return Map(&*cast<Global>(value));
    case Value::Kind::EXPR: return Map(&*cast<Expr>(value));
    case Value::Kind::CONST: return Map(&*cast<Constant>(value));
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
Global *CloneVisitor::Map(Global *global)
{
  switch (global->GetKind()) {
    case Global::Kind::EXTERN: return Map(static_cast<Extern *>(global));
    case Global::Kind::FUNC:   return Map(static_cast<Func *>(global));
    case Global::Kind::BLOCK:  return Map(static_cast<Block *>(global));
    case Global::Kind::ATOM:   return Map(static_cast<Atom *>(global));
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
Expr *CloneVisitor::Map(Expr *expr)
{
  switch (expr->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto *symOff = static_cast<SymbolOffsetExpr *>(expr);
      if (auto *sym = symOff->GetSymbol()) {
        return new SymbolOffsetExpr(Map(sym), symOff->GetOffset());
      } else {
        return new SymbolOffsetExpr(nullptr, symOff->GetOffset());
      }
    }
  }
  llvm_unreachable("invalid expression kind");
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(Inst *i)
{
  switch (i->GetKind()) {
    case Inst::Kind::CALL:          return Clone(static_cast<CallInst *>(i));
    case Inst::Kind::TAIL_CALL:         return Clone(static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:        return Clone(static_cast<InvokeInst *>(i));
    case Inst::Kind::SYSCALL:       return Clone(static_cast<SyscallInst *>(i));
    case Inst::Kind::CLONE:         return Clone(static_cast<CloneInst *>(i));
    case Inst::Kind::RETURN:           return Clone(static_cast<ReturnInst *>(i));
    case Inst::Kind::JUMP_COND:           return Clone(static_cast<JumpCondInst *>(i));
    case Inst::Kind::RAISE:         return Clone(static_cast<RaiseInst *>(i));
    case Inst::Kind::LANDING_PAD:   return Clone(static_cast<LandingPadInst *>(i));
    case Inst::Kind::JUMP:           return Clone(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:        return Clone(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:          return Clone(static_cast<TrapInst *>(i));
    case Inst::Kind::LD:            return Clone(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:            return Clone(static_cast<StoreInst *>(i));
    case Inst::Kind::VASTART:       return Clone(static_cast<VAStartInst *>(i));
    case Inst::Kind::FRAME:         return Clone(static_cast<FrameInst *>(i));
    case Inst::Kind::ALLOCA:        return Clone(static_cast<AllocaInst *>(i));
    case Inst::Kind::SELECT:        return Clone(static_cast<SelectInst *>(i));
    case Inst::Kind::ABS:           return Clone(static_cast<AbsInst *>(i));
    case Inst::Kind::NEG:           return Clone(static_cast<NegInst *>(i));
    case Inst::Kind::SQRT:          return Clone(static_cast<SqrtInst *>(i));
    case Inst::Kind::SIN:           return Clone(static_cast<SinInst *>(i));
    case Inst::Kind::COS:           return Clone(static_cast<CosInst *>(i));
    case Inst::Kind::SEXT:          return Clone(static_cast<SExtInst *>(i));
    case Inst::Kind::ZEXT:          return Clone(static_cast<ZExtInst *>(i));
    case Inst::Kind::XEXT:          return Clone(static_cast<XExtInst *>(i));
    case Inst::Kind::FEXT:          return Clone(static_cast<FExtInst *>(i));
    case Inst::Kind::MOV:           return Clone(static_cast<MovInst *>(i));
    case Inst::Kind::TRUNC:         return Clone(static_cast<TruncInst *>(i));
    case Inst::Kind::EXP:           return Clone(static_cast<ExpInst *>(i));
    case Inst::Kind::EXP2:          return Clone(static_cast<Exp2Inst *>(i));
    case Inst::Kind::LOG:           return Clone(static_cast<LogInst *>(i));
    case Inst::Kind::LOG2:          return Clone(static_cast<Log2Inst *>(i));
    case Inst::Kind::LOG10:         return Clone(static_cast<Log10Inst *>(i));
    case Inst::Kind::FCEIL:         return Clone(static_cast<FCeilInst *>(i));
    case Inst::Kind::FFLOOR:        return Clone(static_cast<FFloorInst *>(i));
    case Inst::Kind::POPCNT:        return Clone(static_cast<PopCountInst *>(i));
    case Inst::Kind::BSWAP:         return Clone(static_cast<BSwapInst *>(i));
    case Inst::Kind::CLZ:           return Clone(static_cast<CLZInst *>(i));
    case Inst::Kind::CTZ:           return Clone(static_cast<CTZInst *>(i));
    case Inst::Kind::CMP:           return Clone(static_cast<CmpInst *>(i));
    case Inst::Kind::UDIV:          return Clone(static_cast<UDivInst *>(i));
    case Inst::Kind::SDIV:          return Clone(static_cast<SDivInst *>(i));
    case Inst::Kind::UREM:          return Clone(static_cast<URemInst *>(i));
    case Inst::Kind::SREM:          return Clone(static_cast<SRemInst *>(i));
    case Inst::Kind::MUL:           return Clone(static_cast<MulInst *>(i));
    case Inst::Kind::ADD:           return Clone(static_cast<AddInst *>(i));
    case Inst::Kind::SUB:           return Clone(static_cast<SubInst *>(i));
    case Inst::Kind::AND:           return Clone(static_cast<AndInst *>(i));
    case Inst::Kind::OR:            return Clone(static_cast<OrInst *>(i));
    case Inst::Kind::ADDSO:         return Clone(static_cast<AddSOInst *>(i));
    case Inst::Kind::MULSO:         return Clone(static_cast<MulSOInst *>(i));
    case Inst::Kind::SUBSO:         return Clone(static_cast<SubSOInst *>(i));
    case Inst::Kind::SLL:           return Clone(static_cast<SllInst *>(i));
    case Inst::Kind::SRA:           return Clone(static_cast<SraInst *>(i));
    case Inst::Kind::SRL:           return Clone(static_cast<SrlInst *>(i));
    case Inst::Kind::XOR:           return Clone(static_cast<XorInst *>(i));
    case Inst::Kind::ROTL:          return Clone(static_cast<RotlInst *>(i));
    case Inst::Kind::ROTR:          return Clone(static_cast<RotrInst *>(i));
    case Inst::Kind::POW:           return Clone(static_cast<PowInst *>(i));
    case Inst::Kind::COPY_SIGN:      return Clone(static_cast<CopySignInst *>(i));
    case Inst::Kind::ADDUO:         return Clone(static_cast<AddUOInst *>(i));
    case Inst::Kind::MULUO:         return Clone(static_cast<MulUOInst *>(i));
    case Inst::Kind::SUBUO:         return Clone(static_cast<SubUOInst *>(i));
    case Inst::Kind::UNDEF:         return Clone(static_cast<UndefInst *>(i));
    case Inst::Kind::PHI:           return Clone(static_cast<PhiInst *>(i));
    case Inst::Kind::ARG:           return Clone(static_cast<ArgInst *>(i));
    case Inst::Kind::SET:           return Clone(static_cast<SetInst *>(i));
    case Inst::Kind::X86_XCHG:      return Clone(static_cast<X86_XchgInst *>(i));
    case Inst::Kind::X86_CMP_XCHG:   return Clone(static_cast<X86_CmpXchgInst *>(i));
    case Inst::Kind::X86_FNSTCW:    return Clone(static_cast<X86_FnStCwInst *>(i));
    case Inst::Kind::X86_FNSTSW:    return Clone(static_cast<X86_FnStSwInst *>(i));
    case Inst::Kind::X86_FNSTENV:   return Clone(static_cast<X86_FnStEnvInst *>(i));
    case Inst::Kind::X86_FLDCW:     return Clone(static_cast<X86_FLdCwInst *>(i));
    case Inst::Kind::X86_FLDENV:    return Clone(static_cast<X86_FLdEnvInst *>(i));
    case Inst::Kind::X86_LDMXCSR:   return Clone(static_cast<X86_LdmXCSRInst *>(i));
    case Inst::Kind::X86_STMXCSR:   return Clone(static_cast<X86_StmXCSRInst *>(i));
    case Inst::Kind::X86_FNCLEX:    return Clone(static_cast<X86_FnClExInst *>(i));
    case Inst::Kind::X86_RDTSC:     return Clone(static_cast<X86_RdtscInst *>(i));
    case Inst::Kind::X86_CPUID:     return Clone(static_cast<X86_CPUIDInst *>(i));
    case Inst::Kind::X86_MFENCE:    return Clone(static_cast<X86_MFenceInst *>(i));
    case Inst::Kind::AARCH64_LL:    return Clone(static_cast<AArch64_LLInst *>(i));
    case Inst::Kind::AARCH64_SC:    return Clone(static_cast<AArch64_SCInst *>(i));
    case Inst::Kind::AARCH64_DMB:   return Clone(static_cast<AArch64_DMBInst *>(i));
    case Inst::Kind::RISCV_XCHG:    return Clone(static_cast<RISCV_XchgInst *>(i));
    case Inst::Kind::RISCV_CMP_XCHG: return Clone(static_cast<RISCV_CmpXchgInst *>(i));
    case Inst::Kind::RISCV_FENCE:   return Clone(static_cast<RISCV_FenceInst *>(i));
    case Inst::Kind::RISCV_GP:      return Clone(static_cast<RISCV_GPInst *>(i));
    case Inst::Kind::PPC_LL:        return Clone(static_cast<PPC_LLInst *>(i));
    case Inst::Kind::PPC_SC:        return Clone(static_cast<PPC_SCInst *>(i));
    case Inst::Kind::PPC_SYNC:      return Clone(static_cast<PPC_SyncInst *>(i));
    case Inst::Kind::PPC_ISYNC:     return Clone(static_cast<PPC_ISyncInst *>(i));
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
AnnotSet CloneVisitor::Annot(const Inst *inst)
{
  return inst->GetAnnots();
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(CallInst *i)
{
  return new CallInst(
      std::vector<Type>{ i->type_begin(), i->type_end() },
      Map(i->GetCallee()),
      CloneArgs<CallInst>(i),
      Map(i->GetCont()),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(TailCallInst *i)
{
  return new TailCallInst(
      std::vector<Type>{ i->type_begin(), i->type_end() },
      Map(i->GetCallee()),
      CloneArgs<TailCallInst>(i),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(InvokeInst *i)
{
  return new InvokeInst(
      std::vector<Type>{ i->type_begin(), i->type_end() },
      Map(i->GetCallee()),
      CloneArgs<InvokeInst>(i),
      Map(i->GetCont()),
      Map(i->GetThrow()),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SyscallInst *i)
{
  return new SyscallInst(
      i->GetType(),
      Map(i->GetSyscall()),
      CloneArgs<SyscallInst>(i),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(CloneInst *i)
{
  return new CloneInst(
      i->GetType(),
      Map(i->GetCallee()),
      Map(i->GetStack()),
      Map(i->GetFlags()),
      Map(i->GetArg()),
      Map(i->GetPTID()),
      Map(i->GetTLS()),
      Map(i->GetCTID()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(ReturnInst *i)
{
  return new ReturnInst(CloneArgs<ReturnInst>(i), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(JumpCondInst *i)
{
  return new JumpCondInst(
      Map(i->GetCond()),
      Map(i->GetTrueTarget()),
      Map(i->GetFalseTarget()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(RaiseInst *i)
{
  return new RaiseInst(
      i->GetCallingConv(),
      Map(i->GetTarget()),
      Map(i->GetStack()),
      CloneArgs<RaiseInst>(i),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(LandingPadInst *i)
{
  return new LandingPadInst(
      i->GetCallingConv(),
      std::vector<Type>{ i->type_begin(), i->type_end() },
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(JumpInst *i)
{
  return new JumpInst(Map(i->GetTarget()), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SwitchInst *i)
{
  std::vector<Block *> branches;
  for (unsigned b = 0; b < i->getNumSuccessors(); ++b) {
    branches.push_back(Map(i->getSuccessor(b)));
  }
  return new SwitchInst(Map(i->GetIdx()), branches, Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(TrapInst *i)
{
  return new TrapInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(LoadInst *i)
{
  return new LoadInst(
      i->GetType(),
      Map(i->GetAddr()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(StoreInst *i)
{
  return new StoreInst(
      Map(i->GetAddr()),
      Map(i->GetVal()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(VAStartInst *i)
{
  return new VAStartInst(Map(i->GetVAList()), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(FrameInst *i)
{
  return new FrameInst(
      i->GetType(),
      new ConstantInt(i->GetObject()),
      new ConstantInt(i->GetOffset()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(AllocaInst *i)
{
  return new AllocaInst(
      i->GetType(),
      Map(i->GetCount()),
      new ConstantInt(i->GetAlign()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SelectInst *i)
{
  return new SelectInst(
      i->GetType(),
      Map(i->GetCond()),
      Map(i->GetTrue()),
      Map(i->GetFalse()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(CmpInst *i)
{
  return new CmpInst(
      i->GetType(),
      i->GetCC(),
      Map(i->GetLHS()),
      Map(i->GetRHS()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(MovInst *i)
{
  return new MovInst(i->GetType(), Map(i->GetArg()), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(UndefInst *i)
{
  return new UndefInst(i->GetType(), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PhiInst *i)
{
  auto *phi = new PhiInst(i->GetType(), Annot(i));
  fixups_.emplace_back(i, phi);
  return phi;
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(ArgInst *i)
{
  return new ArgInst(i->GetType(), new ConstantInt(i->GetIdx()), Annot(i));
}
// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SetInst *i)
{
  return new SetInst(i->GetReg(), Map(i->GetValue()), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(X86_XchgInst *i)
{
  return new X86_XchgInst(
      i->GetType(),
      Map(i->GetAddr()),
      Map(i->GetVal()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(X86_CmpXchgInst *i)
{
  return new X86_CmpXchgInst(
      i->GetType(),
      Map(i->GetAddr()),
      Map(i->GetVal()),
      Map(i->GetRef()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(X86_RdtscInst *i)
{
  return new X86_RdtscInst(i->GetType(), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(X86_FnClExInst *i)
{
  return new X86_FnClExInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(X86_MFenceInst *i)
{
  return new X86_MFenceInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(X86_CPUIDInst *i)
{
  if (i->HasSubleaf()) {
    return new X86_CPUIDInst(
        std::vector<Type>{ i->type_begin(), i->type_end() },
        i->GetLeaf(),
        i->GetSubleaf(),
        Annot(i)
    );
  } else {
    return new X86_CPUIDInst(
        std::vector<Type>{ i->type_begin(), i->type_end() },
        i->GetLeaf(),
        Annot(i)
    );
  }
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(AArch64_LLInst *i)
{
  return new AArch64_LLInst(i->GetType(), Map(i->GetAddr()), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(AArch64_SCInst *i)
{
  return new AArch64_SCInst(
      i->GetType(),
      Map(i->GetAddr()),
      Map(i->GetValue()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(AArch64_DMBInst *i)
{
  return new AArch64_DMBInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(RISCV_XchgInst *i)
{
  return new RISCV_XchgInst(
      i->GetType(),
      Map(i->GetAddr()),
      Map(i->GetVal()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(RISCV_CmpXchgInst *i)
{
  return new RISCV_CmpXchgInst(
      i->GetType(),
      Map(i->GetAddr()),
      Map(i->GetVal()),
      Map(i->GetRef()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(RISCV_FenceInst *i)
{
  return new RISCV_FenceInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(RISCV_GPInst *i)
{
  return new RISCV_GPInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PPC_LLInst *i)
{
  return new PPC_LLInst(i->GetType(), Map(i->GetAddr()), Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PPC_SCInst *i)
{
  return new PPC_SCInst(
      i->GetType(),
      Map(i->GetAddr()),
      Map(i->GetValue()),
      Annot(i)
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PPC_SyncInst *i)
{
  return new PPC_SyncInst(Annot(i));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PPC_ISyncInst *i)
{
  return new PPC_ISyncInst(Annot(i));
}

// -----------------------------------------------------------------------------
class ProgramCloneVisitor : public CloneVisitor {
public:
  ~ProgramCloneVisitor();

  std::pair<std::unique_ptr<Prog>, Inst *> Clone(Prog *oldProg, Inst *inst);

  Ref<Inst> Map(Ref<Inst> inst) override
  {
    if (auto it = insts_.find(inst); it != insts_.end()) {
      return it->second;
    }
    llvm_unreachable("instruction not duplicated");
  }

  Block *Map(Block *oldBlock) override
  {
    auto it = globals_.emplace(oldBlock, nullptr);
    if (it.second) {
      auto *newBlock = new Block(oldBlock->GetName());
      it.first->second = newBlock;
      return newBlock;
    } else {
      return ::cast<Block>(it.first->second);
    }
  }

  Func *Map(Func *oldFunc) override
  {
    auto it = globals_.emplace(oldFunc, nullptr);
    if (it.second) {
      auto *newFunc = new Func(
          oldFunc->GetName(),
          oldFunc->GetVisibility()
      );
      it.first->second = newFunc;
      return newFunc;
    } else {
      return ::cast<Func>(it.first->second);
    }
  }

  Extern *Map(Extern *oldExt) override
  {
    auto it = globals_.emplace(oldExt, nullptr);
    if (it.second) {
      auto *newExt = new Extern(
          oldExt->GetName(),
          oldExt->GetVisibility()
      );
      it.first->second = newExt;
      return newExt;
    } else {
      return ::cast<Extern>(it.first->second);
    }
  }

  Atom *Map(Atom *oldAtom) override
  {
    auto it = globals_.emplace(oldAtom, nullptr);
    if (it.second) {
      auto *newAtom = new Atom(
          oldAtom->GetName(),
          oldAtom->GetVisibility(),
          oldAtom->GetAlignment()
      );
      it.first->second = newAtom;
      return newAtom;
    } else {
      return ::cast<Atom>(it.first->second);
    }
  }

  Constant *Map(Constant *oldConst) override
  {
    switch (oldConst->GetKind()) {
      case Constant::Kind::INT: {
        return new ConstantInt(static_cast<ConstantInt *>(oldConst)->GetValue());
      }
      case Constant::Kind::FLOAT: {
        return new ConstantFloat(static_cast<ConstantFloat *>(oldConst)->GetValue());
      }
      case Constant::Kind::REG: {
        return new ConstantReg(static_cast<ConstantReg *>(oldConst)->GetValue());
      }
    }
    llvm_unreachable("invalid constant kind");
  }

private:
  std::unordered_map<Global *, Global *> globals_;
  std::unordered_map<Ref<Inst>, Ref<Inst>> insts_;
};

// -----------------------------------------------------------------------------
ProgramCloneVisitor::~ProgramCloneVisitor()
{
}

// -----------------------------------------------------------------------------
std::pair<std::unique_ptr<Prog>, Inst *>
ProgramCloneVisitor::Clone(Prog *oldProg, Inst *inst)
{
  auto newProg = std::make_unique<Prog>(oldProg->GetName());

  for (Extern &oldExt : oldProg->externs()) {
    newProg->AddExtern(Map(&oldExt));
  }

  for (Data &oldData : oldProg->data()) {
    Data *newData = new Data(oldData.GetName());
    newProg->AddData(newData);
    for (Object &oldObject : oldData) {
      Object *newObject = new Object();
      newData->AddObject(newObject);
      for (Atom &oldAtom : oldObject) {
        Atom *newAtom = Map(&oldAtom);
        newObject->AddAtom(newAtom);
        for (Item &oldItem : oldAtom) {
          switch (oldItem.GetKind()) {
            case Item::Kind::INT8:
              newAtom->AddItem(new Item(oldItem.GetInt8()));
              break;
            case Item::Kind::INT16:
              newAtom->AddItem(new Item(oldItem.GetInt16()));
              break;
            case Item::Kind::INT32:
              newAtom->AddItem(new Item(oldItem.GetInt32()));
              break;
            case Item::Kind::INT64:
              newAtom->AddItem(new Item(oldItem.GetInt64()));
              break;
            case Item::Kind::FLOAT64:
              newAtom->AddItem(new Item(oldItem.GetFloat64()));
              break;
            case Item::Kind::EXPR:
              newAtom->AddItem(new Item(CloneVisitor::Map(oldItem.GetExpr())));
              break;
            case Item::Kind::ALIGN:
              newAtom->AddItem(new Item(Item::Align{ oldItem.GetAlign() }));
              break;
            case Item::Kind::SPACE:
              newAtom->AddItem(new Item(Item::Space{ oldItem.GetSpace() }));
              break;
            case Item::Kind::STRING:
              newAtom->AddItem(new Item(oldItem.GetString()));
              break;
          }
        }
      }
    }
  }
  Inst *mappedInst = nullptr;
  for (Func &oldFunc : oldProg->funcs()) {
    Func *newFunc = Map(&oldFunc);
    newFunc->SetCallingConv(oldFunc.GetCallingConv());
    newFunc->SetParameters(oldFunc.params());
    newFunc->SetVarArg(oldFunc.IsVarArg());
    newFunc->SetNoInline(oldFunc.IsNoInline());
    if (auto align = oldFunc.GetAlignment()) {
      newFunc->SetAlignment(*align);
    }
    for (auto &obj : oldFunc.objects()) {
      newFunc->AddStackObject(obj.Index, obj.Size, obj.Alignment);
    }
    llvm::ReversePostOrderTraversal<Func*> rpot(&oldFunc);
    for (auto *oldBlock : rpot) {
      Block *newBlock = Map(oldBlock);
      newFunc->AddBlock(newBlock);
      for (auto &oldInst : *oldBlock) {
        auto *newInst = CloneVisitor::Clone(&oldInst);
        if (&oldInst == inst) {
          mappedInst = newInst;
        }
        assert(oldInst.GetNumRets() == newInst->GetNumRets() && "bad clone");
        for (unsigned i = 0, n = oldInst.GetNumRets(); i < n; ++i) {
          insts_.emplace(oldInst.GetSubValue(i), newInst->GetSubValue(i));
        }
        newBlock->AddInst(newInst);
      }
    }
    newProg->AddFunc(newFunc);
    CloneVisitor::Fixup();
    insts_.clear();
  }

  return { std::move(newProg), mappedInst };
}

// -----------------------------------------------------------------------------
std::pair<std::unique_ptr<Prog>, Inst *> Clone(Prog &oldProg, Inst *inst)
{
  return ProgramCloneVisitor().Clone(&oldProg, inst);
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Clone(Prog &oldProg)
{
  return std::move(ProgramCloneVisitor().Clone(&oldProg, nullptr).first);
}
