// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/clone.h"


// -----------------------------------------------------------------------------
InstClone::~InstClone()
{
}

// -----------------------------------------------------------------------------
Block *InstClone::Clone(Block *block)
{
  if (auto [it, inserted] = blocks_.emplace(block, nullptr); inserted) {
    it->second = Make(block);
    return it->second;
  } else {
    return it->second;
  }
}

// -----------------------------------------------------------------------------
Inst *InstClone::Clone(Inst *inst)
{
  if (auto [it, inserted] = insts_.emplace(inst, nullptr); inserted) {
    it->second = Make(inst);
    return it->second;
  } else {
    return it->second;
  }
}

// -----------------------------------------------------------------------------
Value *InstClone::Clone(Value *value)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      return Clone(static_cast<Inst *>(value));
    }
    case Value::Kind::GLOBAL: {
      switch (static_cast<Global *>(value)->GetKind()) {
        case Global::Kind::SYMBOL: return value;
        case Global::Kind::EXTERN: return value;
        case Global::Kind::FUNC: return value;
        case Global::Kind::BLOCK: return Clone(static_cast<Block *>(value));
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
Inst *InstClone::Make(Inst *i)
{
  switch (i->GetKind()) {
    case Inst::Kind::CALL:     return Make(static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:    return Make(static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:   return Make(static_cast<InvokeInst *>(i));
    case Inst::Kind::TINVOKE:  return Make(static_cast<TailInvokeInst *>(i));
    case Inst::Kind::RET:      return Make(static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:      return Make(static_cast<JumpCondInst *>(i));
    case Inst::Kind::JI:       return Make(static_cast<JumpIndirectInst *>(i));
    case Inst::Kind::JMP:      return Make(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:   return Make(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:     return Make(static_cast<TrapInst *>(i));
    case Inst::Kind::LD:       return Make(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:       return Make(static_cast<StoreInst *>(i));
    case Inst::Kind::XCHG:     return Make(static_cast<ExchangeInst *>(i));
    case Inst::Kind::SET:      return Make(static_cast<SetInst *>(i));
    case Inst::Kind::VASTART:  return Make(static_cast<VAStartInst *>(i));
    case Inst::Kind::FRAME:    return Make(static_cast<FrameInst *>(i));
    case Inst::Kind::SELECT:   return Make(static_cast<SelectInst *>(i));
    case Inst::Kind::ABS:      return Make(static_cast<AbsInst *>(i));
    case Inst::Kind::NEG:      return Make(static_cast<NegInst *>(i));
    case Inst::Kind::SQRT:     return Make(static_cast<SqrtInst *>(i));
    case Inst::Kind::SIN:      return Make(static_cast<SinInst *>(i));
    case Inst::Kind::COS:      return Make(static_cast<CosInst *>(i));
    case Inst::Kind::SEXT:     return Make(static_cast<SExtInst *>(i));
    case Inst::Kind::ZEXT:     return Make(static_cast<ZExtInst *>(i));
    case Inst::Kind::FEXT:     return Make(static_cast<FExtInst *>(i));
    case Inst::Kind::MOV:      return Make(static_cast<MovInst *>(i));
    case Inst::Kind::TRUNC:    return Make(static_cast<TruncInst *>(i));
    case Inst::Kind::CMP:      return Make(static_cast<CmpInst *>(i));
    case Inst::Kind::DIV:      return Make(static_cast<DivInst *>(i));
    case Inst::Kind::REM:      return Make(static_cast<RemInst *>(i));
    case Inst::Kind::MUL:      return Make(static_cast<MulInst *>(i));
    case Inst::Kind::ADD:      return Make(static_cast<AddInst *>(i));
    case Inst::Kind::SUB:      return Make(static_cast<SubInst *>(i));
    case Inst::Kind::AND:      return Make(static_cast<AndInst *>(i));
    case Inst::Kind::OR:       return Make(static_cast<OrInst *>(i));
    case Inst::Kind::SLL:      return Make(static_cast<SllInst *>(i));
    case Inst::Kind::SRA:      return Make(static_cast<SraInst *>(i));
    case Inst::Kind::SRL:      return Make(static_cast<SrlInst *>(i));
    case Inst::Kind::XOR:      return Make(static_cast<XorInst *>(i));
    case Inst::Kind::ROTL:     return Make(static_cast<RotlInst *>(i));
    case Inst::Kind::POW:      return Make(static_cast<PowInst *>(i));
    case Inst::Kind::COPYSIGN: return Make(static_cast<CopySignInst *>(i));
    case Inst::Kind::UADDO:    return Make(static_cast<AddUOInst *>(i));
    case Inst::Kind::UMULO:    return Make(static_cast<MulUOInst *>(i));
    case Inst::Kind::UNDEF:    return Make(static_cast<UndefInst *>(i));
    case Inst::Kind::PHI:      return Make(static_cast<PhiInst *>(i));
    case Inst::Kind::ARG:      return Make(static_cast<ArgInst *>(i));
  }
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(CallInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(TailCallInst *i)
{
  return new TailCallInst(
      i->GetType(),
      Clone(i->GetCallee()),
      MakeArgs<TailCallInst>(i),
      i->GetNumFixedArgs(),
      i->GetCallingConv(),
      i->GetAnnotation()
  );
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(InvokeInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(TailInvokeInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(ReturnInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(JumpCondInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(JumpIndirectInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(JumpInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(SwitchInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(TrapInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(LoadInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(StoreInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(ExchangeInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(SetInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(VAStartInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(FrameInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(SelectInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(CmpInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(MovInst *i)
{
  return new MovInst(i->GetType(), Clone(i->GetArg()), i->GetAnnotation());
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(UndefInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(PhiInst *i)
{
  assert(!"not implemented");
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *InstClone::Make(ArgInst *i)
{
  return new ArgInst(i->GetType(), new ConstantInt(i->GetIdx()));
}

// -----------------------------------------------------------------------------
template<typename T>
Inst *InstClone::MakeBinary(BinaryInst *i)
{
  return new T(i->GetType(), Clone(i->GetLHS()), Clone(i->GetRHS()));
}

// -----------------------------------------------------------------------------
template<typename T>
Inst *InstClone::MakeUnary(UnaryInst *i)
{
  return new T(i->GetType(), Clone(i->GetArg()));
}

// -----------------------------------------------------------------------------
template<typename T>
Inst *InstClone::MakeOverflow(OverflowInst *i)
{
  return new T(Clone(i->GetLHS()), Clone(i->GetRHS()));
}

// -----------------------------------------------------------------------------
template<typename T>
std::vector<Inst *> InstClone::MakeArgs(T *inst)
{
  std::vector<Inst *> args;
  for (auto *arg : inst->args()) {
    args.push_back(Clone(arg));
  }
  return args;
}
