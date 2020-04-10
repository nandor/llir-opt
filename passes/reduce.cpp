// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/RandomNumberGenerator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/reduce.h"


// -----------------------------------------------------------------------------
template <typename T, typename Gen>
T PickOne(const std::vector<T> &items, Gen &gen)
{
  return items[(std::uniform_int_distribution<>(0, items.size() - 1))(gen)];
}

// -----------------------------------------------------------------------------
void ReducePass::Run(Prog *prog)
{
  if (prog->begin() == prog->end()) {
    return;
  }

  // Pick a function to work on.
  Func *f;
  {
    std::vector<Func *> funcs;
    for (Func &f : *prog) {
      funcs.push_back(&f);
    }
    f = PickOne(funcs, rand_);
  }

  // Pick an instruction to mutate.
  Inst *i;
  {
    std::vector<Inst *> insts;
    for (Block &block : *f) {
      for (Inst &i : block) {
        insts.push_back(&i);
      }
    }
    i = PickOne(insts, rand_);
  }

  // Mutate the instruction based on its kind.
  switch (i->GetKind()) {
    case Inst::Kind::CALL:      return ReduceCall(static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:     llvm_unreachable("TCALL");
    case Inst::Kind::INVOKE:    llvm_unreachable("INVOKE");
    case Inst::Kind::TINVOKE:   llvm_unreachable("TINVOKE");
    case Inst::Kind::RET:       return ReduceRet(static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:       return ReduceJcc(static_cast<JumpCondInst *>(i));
    case Inst::Kind::JI:        llvm_unreachable("JI");
    case Inst::Kind::JMP:       return ReduceJmp(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:    return ReduceSwitch(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:      return;
    case Inst::Kind::LD:        return ReduceLoad(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:        return ReduceStore(static_cast<StoreInst *>(i));
    case Inst::Kind::XCHG:      llvm_unreachable("XCHG");
    case Inst::Kind::SET:       llvm_unreachable("SET");
    case Inst::Kind::VASTART:   llvm_unreachable("VASTART");
    case Inst::Kind::ALLOCA:    llvm_unreachable("ALLOCA");
    case Inst::Kind::ARG:       return ReduceArg(static_cast<ArgInst *>(i));
    case Inst::Kind::FRAME:     llvm_unreachable("FRAME");
    case Inst::Kind::UNDEF:     return;
    case Inst::Kind::RDTSC:     llvm_unreachable("RDTSC");
    case Inst::Kind::SELECT:    llvm_unreachable("SELECT");
    case Inst::Kind::ABS:       llvm_unreachable("ABS");
    case Inst::Kind::NEG:       llvm_unreachable("NEG");
    case Inst::Kind::SQRT:      llvm_unreachable("SQRT");
    case Inst::Kind::SIN:       llvm_unreachable("SIN");
    case Inst::Kind::COS:       llvm_unreachable("COS");
    case Inst::Kind::SEXT:      llvm_unreachable("SEXT");
    case Inst::Kind::ZEXT:      llvm_unreachable("ZEXT");
    case Inst::Kind::FEXT:      llvm_unreachable("FEXT");
    case Inst::Kind::MOV:       return ReduceMov(static_cast<MovInst *>(i));
    case Inst::Kind::TRUNC:     llvm_unreachable("TRUNC");
    case Inst::Kind::EXP:       llvm_unreachable("EXP");
    case Inst::Kind::EXP2:      llvm_unreachable("EXP2");
    case Inst::Kind::LOG:       llvm_unreachable("LOG");
    case Inst::Kind::LOG2:      llvm_unreachable("LOG2");
    case Inst::Kind::LOG10:     llvm_unreachable("LOG10");
    case Inst::Kind::FCEIL:     llvm_unreachable("FCEIL");
    case Inst::Kind::FFLOOR:    llvm_unreachable("FFLOOR");
    case Inst::Kind::POPCNT:    llvm_unreachable("POPCNT");
    case Inst::Kind::CLZ:       llvm_unreachable("CLZ");
    case Inst::Kind::ADD:       return ReduceBinary(i);
    case Inst::Kind::AND:       return ReduceBinary(i);
    case Inst::Kind::CMP:       return ReduceBinary(i);
    case Inst::Kind::DIV:       return ReduceBinary(i);
    case Inst::Kind::REM:       return ReduceBinary(i);
    case Inst::Kind::MUL:       return ReduceBinary(i);
    case Inst::Kind::OR:        return ReduceBinary(i);
    case Inst::Kind::ROTL:      return ReduceBinary(i);
    case Inst::Kind::SLL:       return ReduceBinary(i);
    case Inst::Kind::SRA:       return ReduceBinary(i);
    case Inst::Kind::SRL:       return ReduceBinary(i);
    case Inst::Kind::SUB:       return ReduceBinary(i);
    case Inst::Kind::XOR:       return ReduceBinary(i);
    case Inst::Kind::POW:       return ReduceBinary(i);
    case Inst::Kind::COPYSIGN:  llvm_unreachable("COPYSIGN");
    case Inst::Kind::UADDO:     return ReduceBinary(i);
    case Inst::Kind::UMULO:     return ReduceBinary(i);
    case Inst::Kind::PHI:       return ReducePhi(static_cast<PhiInst *>(i));
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceArg(ArgInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceCall(CallInst *i)
{
  if (auto ty = i->GetType()) {
    switch (Random(0)) {
      case 0: return ReduceUndefined(i);
    }
  } else {
    switch (Random(0)) {
      case 0: return ReduceErase(i);
    }
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceLoad(LoadInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceStore(StoreInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceErase(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceMov(MovInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceBinary(Inst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceSwitch(SwitchInst *i)
{
  unsigned n = i->getNumSuccessors();
  assert(n > 0 && "invalid switch");

  Block *from = i->getParent();
  if (n == 1) {
    Block *to = i->getSuccessor(0);
    from->AddInst(new TrapInst({}), i);
    i->eraseFromParent();
    RemoveEdge(from, to);
  } else {
    std::vector<Block *> branches;
    Block *to = nullptr;
    unsigned index = Random(n - 1);
    for (unsigned j = 0; j < n; ++j) {
      Block *succ = i->getSuccessor(j);
      if (j != index) {
        branches.push_back(succ);
      } else {
        assert(!to);
        to = succ;
      }
    }
    assert(to && "missing branch to delete");
    from->AddInst(new SwitchInst(i->GetIdx(), branches, i->GetAnnot()));
    i->eraseFromParent();
    RemoveEdge(from, to);
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceJmp(JumpInst *i)
{
  Block *from = i->getParent();
  Block *to = i->GetTarget();
  from->AddInst(new TrapInst({}), i);
  i->eraseFromParent();
  RemoveEdge(from, to);
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceJcc(JumpCondInst *i)
{
  bool flag = Random(1);

  Block *from = i->getParent();
  Block *to = flag ? i->GetTrueTarget() : i->GetFalseTarget();
  Block *other = flag ? i->GetFalseTarget() : i->GetTrueTarget();

  from->AddInst(new JumpInst(to, i->GetAnnot()), i);
  i->eraseFromParent();
  RemoveEdge(from, other);
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceRet(ReturnInst *i)
{
  i->getParent()->AddInst(new TrapInst({}), i);
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::ReducePhi(PhiInst *phi)
{
  unsigned n = phi->GetNumIncoming();
  unsigned i = Random(n);
  if (i == n) {
    Block *parent = phi->getParent();
    auto it = parent->begin();
    while (it != parent->end() && it->Is(Inst::Kind::PHI))
      ++it;
    auto *undef = new UndefInst(phi->GetType(0), phi->GetAnnot());
    parent->AddInst(undef, &*it);
    phi->replaceAllUsesWith(undef);
    phi->eraseFromParent();
  } else {
    Constant *cst = nullptr;
    switch (phi->GetType(0)) {
      case Type::I8:
      case Type::I16:
      case Type::I32:
      case Type::I64:
      case Type::I128:
      case Type::U8:
      case Type::U16:
      case Type::U32:
      case Type::U64:
      case Type::U128: {
        phi->SetValue(i, new ConstantInt(0));
        return;
      }
      case Type::F32:
      case Type::F64:
      case Type::F80: {
        phi->SetValue(i, new ConstantFloat(0.0));
        return;
      }
    }
    llvm_unreachable("invalid phi type");
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceUndefined(Inst *i)
{
  AnnotSet annot = i->GetAnnot();
  annot.Clear(CAML_FRAME);
  annot.Clear(CAML_VALUE);

  auto *undef = new UndefInst(i->GetType(0), annot);
  i->getParent()->AddInst(undef, i);
  i->replaceAllUsesWith(undef);
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceErase(Inst *i)
{
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::RemoveEdge(Block *from, Block *to)
{
  for (PhiInst &phi : to->phis()) {
    if (phi.GetNumIncoming() == 1) {
      llvm_unreachable("not implemented");
    } else {
      phi.Remove(from);
    }
  }
}

// -----------------------------------------------------------------------------
unsigned ReducePass::Random(unsigned n)
{
  return (std::uniform_int_distribution<>(0, n))(rand_);
}

// -----------------------------------------------------------------------------
const char *ReducePass::GetPassName() const
{
  return "Test Reducer";
}
