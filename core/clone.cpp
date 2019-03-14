// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/clone.h"


// -----------------------------------------------------------------------------
CloneVisitor::~CloneVisitor()
{
}

// -----------------------------------------------------------------------------
Value *CloneVisitor::Map(Value *value)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      return Map(static_cast<Inst *>(value));
    }
    case Value::Kind::GLOBAL: {
      switch (static_cast<Global *>(value)->GetKind()) {
        case Global::Kind::SYMBOL: return value;
        case Global::Kind::EXTERN: return value;
        case Global::Kind::FUNC: return value;
        case Global::Kind::BLOCK: return Map(static_cast<Block *>(value));
        case Global::Kind::ATOM: return value;
      }
    }
    case Value::Kind::EXPR: {
      return value;
    }
    case Value::Kind::CONST: {
      return value;
    }
  }
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(Inst *i)
{
  switch (i->GetKind()) {
    case Inst::Kind::CALL:     return Clone(static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:    return Clone(static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:   return Clone(static_cast<InvokeInst *>(i));
    case Inst::Kind::TINVOKE:  return Clone(static_cast<TailInvokeInst *>(i));
    case Inst::Kind::RET:      return Clone(static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:      return Clone(static_cast<JumpCondInst *>(i));
    case Inst::Kind::JI:       return Clone(static_cast<JumpIndirectInst *>(i));
    case Inst::Kind::JMP:      return Clone(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:   return Clone(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:     return Clone(static_cast<TrapInst *>(i));
    case Inst::Kind::LD:       return Clone(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:       return Clone(static_cast<StoreInst *>(i));
    case Inst::Kind::XCHG:     return Clone(static_cast<ExchangeInst *>(i));
    case Inst::Kind::SET:      return Clone(static_cast<SetInst *>(i));
    case Inst::Kind::VASTART:  return Clone(static_cast<VAStartInst *>(i));
    case Inst::Kind::FRAME:    return Clone(static_cast<FrameInst *>(i));
    case Inst::Kind::SELECT:   return Clone(static_cast<SelectInst *>(i));
    case Inst::Kind::ABS:      return Clone(static_cast<AbsInst *>(i));
    case Inst::Kind::NEG:      return Clone(static_cast<NegInst *>(i));
    case Inst::Kind::SQRT:     return Clone(static_cast<SqrtInst *>(i));
    case Inst::Kind::SIN:      return Clone(static_cast<SinInst *>(i));
    case Inst::Kind::COS:      return Clone(static_cast<CosInst *>(i));
    case Inst::Kind::SEXT:     return Clone(static_cast<SExtInst *>(i));
    case Inst::Kind::ZEXT:     return Clone(static_cast<ZExtInst *>(i));
    case Inst::Kind::FEXT:     return Clone(static_cast<FExtInst *>(i));
    case Inst::Kind::MOV:      return Clone(static_cast<MovInst *>(i));
    case Inst::Kind::TRUNC:    return Clone(static_cast<TruncInst *>(i));
    case Inst::Kind::CMP:      return Clone(static_cast<CmpInst *>(i));
    case Inst::Kind::DIV:      return Clone(static_cast<DivInst *>(i));
    case Inst::Kind::REM:      return Clone(static_cast<RemInst *>(i));
    case Inst::Kind::MUL:      return Clone(static_cast<MulInst *>(i));
    case Inst::Kind::ADD:      return Clone(static_cast<AddInst *>(i));
    case Inst::Kind::SUB:      return Clone(static_cast<SubInst *>(i));
    case Inst::Kind::AND:      return Clone(static_cast<AndInst *>(i));
    case Inst::Kind::OR:       return Clone(static_cast<OrInst *>(i));
    case Inst::Kind::SLL:      return Clone(static_cast<SllInst *>(i));
    case Inst::Kind::SRA:      return Clone(static_cast<SraInst *>(i));
    case Inst::Kind::SRL:      return Clone(static_cast<SrlInst *>(i));
    case Inst::Kind::XOR:      return Clone(static_cast<XorInst *>(i));
    case Inst::Kind::ROTL:     return Clone(static_cast<RotlInst *>(i));
    case Inst::Kind::POW:      return Clone(static_cast<PowInst *>(i));
    case Inst::Kind::COPYSIGN: return Clone(static_cast<CopySignInst *>(i));
    case Inst::Kind::UADDO:    return Clone(static_cast<AddUOInst *>(i));
    case Inst::Kind::UMULO:    return Clone(static_cast<MulUOInst *>(i));
    case Inst::Kind::UNDEF:    return Clone(static_cast<UndefInst *>(i));
    case Inst::Kind::PHI:      return Clone(static_cast<PhiInst *>(i));
    case Inst::Kind::ARG:      return Clone(static_cast<ArgInst *>(i));
  }
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(CallInst *i)
{
  return new CallInst(
      i->GetType(),
      Map(i->GetCallee()),
      CloneArgs<CallInst>(i),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      i->GetAnnotation()
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(TailCallInst *i)
{
  return new TailCallInst(
      i->GetType(),
      Map(i->GetCallee()),
      CloneArgs<TailCallInst>(i),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      i->GetAnnotation()
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(InvokeInst *i)
{
  return new InvokeInst(
      i->GetType(),
      Map(i->GetCallee()),
      CloneArgs<InvokeInst>(i),
      Map(i->GetCont()),
      Map(i->GetThrow()),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      i->GetAnnotation()
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(TailInvokeInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(ReturnInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(JumpCondInst *i)
{
  return new JumpCondInst(
      Map(i->GetCond()),
      Map(i->GetTrueTarget()),
      Map(i->GetFalseTarget())
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(JumpIndirectInst *i)
{
  return new JumpIndirectInst(Map(i->GetTarget()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(JumpInst *i)
{
  return new JumpInst(Map(i->GetTarget()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SwitchInst *i)
{
  std::vector<Block *> branches;
  for (unsigned b = 0; b < i->getNumSuccessors(); ++b) {
    branches.push_back(Map(i->getSuccessor(b)));
  }
  return new SwitchInst(Map(i->GetIdx()), branches);
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(TrapInst *i)
{
  return new TrapInst();
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(LoadInst *i)
{
  return new LoadInst(i->GetLoadSize(), i->GetType(), Map(i->GetAddr()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(StoreInst *i)
{
  return new StoreInst(i->GetStoreSize(), Map(i->GetAddr()), Map(i->GetVal()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(ExchangeInst *i)
{
  return new ExchangeInst(i->GetType(), Map(i->GetAddr()), Map(i->GetVal()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SetInst *i)
{
  return new SetInst(i->GetReg(), Map(i->GetValue()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(VAStartInst *i)
{
  return new VAStartInst(Map(i->GetVAList()));
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(FrameInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(SelectInst *i)
{
  return new SelectInst(
      i->GetType(),
      Map(i->GetCond()),
      Map(i->GetTrue()),
      Map(i->GetFalse())
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(CmpInst *i)
{
  return new CmpInst(
      i->GetType(),
      i->GetCC(),
      Map(i->GetLHS()),
      Map(i->GetRHS())
  );
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(MovInst *i)
{
  return new MovInst(i->GetType(), Map(i->GetArg()), i->GetAnnotation());
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(UndefInst *i)
{
  return new UndefInst(i->GetType());
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PhiInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(ArgInst *i)
{
  return new ArgInst(i->GetType(), new ConstantInt(i->GetIdx()));
}

// -----------------------------------------------------------------------------
template<typename T>
Inst *CloneVisitor::CloneBinary(BinaryInst *i)
{
  return new T(i->GetType(), Map(i->GetLHS()), Map(i->GetRHS()));
}

// -----------------------------------------------------------------------------
template<typename T>
Inst *CloneVisitor::CloneUnary(UnaryInst *i)
{
  return new T(i->GetType(), Map(i->GetArg()));
}

// -----------------------------------------------------------------------------
template<typename T>
Inst *CloneVisitor::CloneOverflow(OverflowInst *i)
{
  return new T(Map(i->GetLHS()), Map(i->GetRHS()));
}

// -----------------------------------------------------------------------------
template<typename T>
std::vector<Inst *> CloneVisitor::CloneArgs(T *inst)
{
  std::vector<Inst *> args;
  for (auto *arg : inst->args()) {
    args.push_back(Map(arg));
  }
  return args;
}
