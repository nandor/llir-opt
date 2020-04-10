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


#include "core/printer.h"
Printer p(llvm::errs());


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
        if (i.HasSideEffects() && !i.IsTerminator()) {
          insts.push_back(&i);
        }
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
    case Inst::Kind::RET:       llvm_unreachable("RET");
    case Inst::Kind::JCC:       llvm_unreachable("JCC");
    case Inst::Kind::JI:        llvm_unreachable("JI");
    case Inst::Kind::JMP:       llvm_unreachable("JMP");
    case Inst::Kind::SWITCH:    llvm_unreachable("SWITCH");
    case Inst::Kind::TRAP:      llvm_unreachable("TRAP");
    case Inst::Kind::LD:        llvm_unreachable("LD");
    case Inst::Kind::ST:        llvm_unreachable("ST");
    case Inst::Kind::XCHG:      llvm_unreachable("XCHG");
    case Inst::Kind::SET:       llvm_unreachable("SET");
    case Inst::Kind::VASTART:   llvm_unreachable("VASTART");
    case Inst::Kind::ALLOCA:    llvm_unreachable("ALLOCA");
    case Inst::Kind::ARG:       llvm_unreachable("ARG");
    case Inst::Kind::FRAME:     llvm_unreachable("FRAME");
    case Inst::Kind::UNDEF:     llvm_unreachable("UNDEF");
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
    case Inst::Kind::MOV:       llvm_unreachable("MOV");
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
    case Inst::Kind::ADD:       llvm_unreachable("ADD");
    case Inst::Kind::AND:       llvm_unreachable("AND");
    case Inst::Kind::CMP:       llvm_unreachable("CMP");
    case Inst::Kind::DIV:       llvm_unreachable("DIV");
    case Inst::Kind::REM:       llvm_unreachable("REM");
    case Inst::Kind::MUL:       llvm_unreachable("MUL");
    case Inst::Kind::OR:        llvm_unreachable("OR");
    case Inst::Kind::ROTL:      llvm_unreachable("ROTL");
    case Inst::Kind::SLL:       llvm_unreachable("SLL");
    case Inst::Kind::SRA:       llvm_unreachable("SRA");
    case Inst::Kind::SRL:       llvm_unreachable("SRL");
    case Inst::Kind::SUB:       llvm_unreachable("SUB");
    case Inst::Kind::XOR:       llvm_unreachable("XOR");
    case Inst::Kind::POW:       llvm_unreachable("POW");
    case Inst::Kind::COPYSIGN:  llvm_unreachable("COPYSIGN");
    case Inst::Kind::UADDO:     llvm_unreachable("UADDO");
    case Inst::Kind::UMULO:     llvm_unreachable("UMULO");
    case Inst::Kind::PHI:       llvm_unreachable("PHI");
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceCall(CallInst *i)
{
  if (auto ty = i->GetType()) {
    switch (Random(0)) {
      case 0: return ReduceUndefined(i);
    }
    llvm_unreachable("missing reducer");
  } else {
    i->removeFromParent();
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceUndefined(Inst *i)
{

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
