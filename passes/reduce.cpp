// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/RandomNumberGenerator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
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
    case Inst::Kind::INVOKE:    return ReduceInvoke(static_cast<InvokeInst *>(i));
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
    case Inst::Kind::FRAME:     return ReduceFrame(static_cast<FrameInst *>(i));
    case Inst::Kind::UNDEF:     return;
    case Inst::Kind::RDTSC:     llvm_unreachable("RDTSC");
    case Inst::Kind::SELECT:    return ReduceSelect(static_cast<SelectInst *>(i));
    case Inst::Kind::ABS:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::NEG:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::SQRT:      return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::SIN:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::COS:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::SEXT:      return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::ZEXT:      return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::FEXT:      return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::MOV:       return ReduceMov(static_cast<MovInst *>(i));
    case Inst::Kind::TRUNC:     return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::EXP:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::EXP2:      return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::LOG:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::LOG2:      return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::LOG10:     return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::FCEIL:     return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::FFLOOR:    return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::POPCNT:    return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::CLZ:       return ReduceUnary(static_cast<UnaryInst *>(i));
    case Inst::Kind::ADD:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::AND:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::CMP:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::DIV:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::REM:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::MUL:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::OR:        return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::ROTL:      return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::ROTR:      return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SLL:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SRA:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SRL:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SUB:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::XOR:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::POW:       return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::COPYSIGN:  llvm_unreachable("COPYSIGN");
    case Inst::Kind::UADDO:     return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::UMULO:     return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::USUBO:     return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SADDO:     return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SMULO:     return ReduceBinary(static_cast<BinaryInst *>(i));
    case Inst::Kind::SSUBO:     return ReduceBinary(static_cast<BinaryInst *>(i));
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
void ReducePass::ReduceFrame(FrameInst *i)
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
void ReducePass::ReduceInvoke(InvokeInst *i)
{
  if (auto ty = i->GetType()) {
    switch (Random(0)) {
      case 0: {
        auto *block = i->getParent();
        auto *branch = Random(1) ? i->GetCont() : i->GetThrow();
        ReduceUndefined(i);
        auto *jump = new JumpInst(branch, {});
        block->AddInst(jump);
        return;
      }
    }
  } else {
    llvm_unreachable("missing reducer");
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
void ReducePass::ReduceUnary(UnaryInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceBinary(BinaryInst *i)
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
    phi.Remove(from);
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceSelect(SelectInst *select)
{
  Inst *arg = nullptr;
  switch (Random(2)) {
    case 0: return ReduceUndefined(select);
    case 1: {
      arg = select->GetTrue();
      break;
    }
    case 2: {
      arg = select->GetFalse();
      break;
    }
  }

  select->replaceAllUsesWith(arg);
  select->eraseFromParent();
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
