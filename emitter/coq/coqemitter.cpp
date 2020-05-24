// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "emitter/coq/coqemitter.h"



// -----------------------------------------------------------------------------
CoqEmitter::CoqEmitter(llvm::raw_ostream &os)
  : os_(os)
{
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(const Prog &prog)
{
  os_ << "Require Import Coq.ZArith.ZArith.\n";
  os_ << "Require Import LLIR.LLIR.\n";
  os_ << "Require Import LLIR.Maps.\n";
  os_ << "Require Import Coq.Lists.List.\n";
  os_ << "Import ListNotations.\n";

  os_ << "\n";
  for (const Func &func : prog) {
    Write(func);
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(const Func &func)
{
  os_ << "Definition " << func.getName() << ": func := \n";

  // Stack object descriptors.
  os_.indent(2) << "{| fn_stack :=\n";
  os_.indent(4) << "<< ";
  {
    auto objs = func.objects();
    for (unsigned i = 0, n = objs.size(); i < n; ++i) {
      if (i != 0) {
        os_ << ";  ";
      }

      auto &obj = objs[i];
      os_ << "(";
      os_ << (obj.Index + 1) << "%positive";
      os_ << ", ";
      os_ << "{| obj_size := " << obj.Size << "%positive";
      os_ << "; obj_align := " << obj.Alignment << "%positive";
      os_ << "|}";
      os_ << ")";
      os_ << "\n";
      os_.indent(4);
    }
  }
  os_ << ">>\n";

  // Build a map of instruction and block indices.
  llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);
  insts_.clear();
  blocks_.clear();
  for (const Block *block : blockOrder) {
    std::optional<unsigned> firstNonPhi;
    for (const Inst &inst : *block) {
      unsigned idx = insts_.size() + 1;
      insts_[&inst] = idx;
      if (!inst.Is(Inst::Kind::PHI) && !firstNonPhi) {
        firstNonPhi = idx;
      }
    }
    assert(firstNonPhi && "empty block");
    blocks_[block] = *firstNonPhi;
  }

  // Instructions.
  std::optional<unsigned> entry;
  os_.indent(2) << "; fn_insts :=\n";
  os_.indent(4) << "<< ";
  {
    for (const Block *block : blockOrder) {
      for (auto it = block->begin(), end = block->end(); it != end; ++it) {
        if (it->Is(Inst::Kind::PHI)) {
          continue;
        }

        unsigned idx = insts_[&*it];
        if (entry) {
          os_ << ";  ";
        } else {
          entry = idx;
        }

        os_ << "(" << idx << "%positive, ";
        Write(it);
        os_ << ")\n";
        os_.indent(4);
      }
    }
  }
  os_ << ">>\n";

  // PHIs for each block.
  os_.indent(2) << "; fn_phis := \n";
  os_.indent(4) << "<<";
  {
    bool first = true;
    for (const Block *block : blockOrder) {
      llvm::SmallVector<const PhiInst *, 10> phis;
      for (const Inst &inst : *block) {
        if (auto *phi = ::dyn_cast_or_null<const PhiInst>(&inst)) {
          phis.push_back(phi);
        } else {
          break;
        }
      }
      if (!phis.empty()) {
        if (!first) {
          os_ << "; ";
        }
        first = false;

        os_ << " (" << blocks_[block] << "%positive\n";
        os_.indent(7) << ", [ ";
        for (unsigned i = 0, n = phis.size(); i < n; ++i) {
          if (i != 0) {
            os_ << "; ";
          }

          const PhiInst *phi = phis[i];
          os_ << "LLPhi\n";
          os_.indent(11) << "[ ";
          for (unsigned j = 0, m = phi->GetNumIncoming(); j < m; ++j) {
            if (j != 0) {
              os_ << "; ";
            }

            const Block *block = phi->GetBlock(j);
            const Inst *value = phi->GetValue(j);
            os_ << "(";
            os_ << insts_[block->GetTerminator()] << "%positive";
            os_ << ", ";
            os_ << insts_[value] << "%positive";
            os_ << ")\n";
            os_.indent(11);
          }
          os_ << "]\n";
          os_.indent(11) << insts_[phi] << "%positive\n";
          os_.indent(9);
        }
        os_ << "]\n";
        os_.indent(7) << ")\n";
        os_.indent(4);
      }
    }
  }
  os_ << ">>\n";

  // Entry point.
  assert(entry && "missing entry point");
  os_.indent(2) << "; fn_entry := " << *entry << "%positive\n";
  os_.indent(2) << "|}.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(Block::const_iterator it)
{
  switch (it->GetKind()) {
    case Inst::Kind::CALL: os_ << "LLJmp xH"; return;
    case Inst::Kind::TCALL:  os_ << "LLJmp xH"; return;
    case Inst::Kind::INVOKE: os_ << "LLJmp xH"; return;
    case Inst::Kind::TINVOKE:  os_ << "LLJmp xH"; return;
    case Inst::Kind::RET:  os_ << "LLJmp xH"; return;
    case Inst::Kind::JCC:  os_ << "LLJmp xH"; return;
    case Inst::Kind::JI: os_ << "LLJmp xH"; return;
    case Inst::Kind::JMP:  os_ << "LLJmp xH"; return;
    case Inst::Kind::SWITCH: os_ << "LLJmp xH"; return;
    case Inst::Kind::TRAP: os_ << "LLJmp xH"; return;
    // Load.
    case Inst::Kind::LD: os_ << "LLJmp xH"; return;
    // Store.
    case Inst::Kind::ST: os_ << "LLJmp xH"; return;
    // Atomic exchange.
    case Inst::Kind::XCHG: os_ << "LLJmp xH"; return;
    // Atomic compare and exchange.
    case Inst::Kind::CMPXCHG:  os_ << "LLJmp xH"; return;
    // Set register.
    case Inst::Kind::SET:  os_ << "LLJmp xH"; return;
    // Variable argument lists.
    case Inst::Kind::VASTART:  os_ << "LLJmp xH"; return;
    // Dynamic stack allcoation.
    case Inst::Kind::ALLOCA: os_ << "LLJmp xH"; return;
    // Argument.
    case Inst::Kind::ARG:  os_ << "LLJmp xH"; return;
    // Frame address.
    case Inst::Kind::FRAME:  os_ << "LLJmp xH"; return;
    // Undefined value.
    case Inst::Kind::UNDEF:  os_ << "LLJmp xH"; return;
    // Hardware instructions.
    case Inst::Kind::RDTSC:  os_ << "LLJmp xH"; return;
    case Inst::Kind::FNSTCW: os_ << "LLJmp xH"; return;
    case Inst::Kind::FLDCW:  os_ << "LLJmp xH"; return;
    case Inst::Kind::SYSCALL:  os_ << "LLJmp xH"; return;
    // Conditional.
    case Inst::Kind::SELECT: os_ << "LLJmp xH"; return;
    // PHI node.
    case Inst::Kind::PHI:  os_ << "LLJmp xH"; return;
    case Inst::Kind::MOV:  os_ << "LLJmp xH"; return;
    // Unary instructions.
    case Inst::Kind::ABS:       return Unary(it, "Abs");
    case Inst::Kind::NEG:       return Unary(it, "Neg");
    case Inst::Kind::SQRT:      return Unary(it, "Sqrt");
    case Inst::Kind::SIN:       return Unary(it, "Sin");
    case Inst::Kind::COS:       return Unary(it, "Cos");
    case Inst::Kind::SEXT:      return Unary(it, "Sext");
    case Inst::Kind::ZEXT:      return Unary(it, "Zext");
    case Inst::Kind::FEXT:      return Unary(it, "Fext");
    case Inst::Kind::XEXT:      return Unary(it, "Xext");
    case Inst::Kind::TRUNC:     return Unary(it, "Trunc");
    case Inst::Kind::EXP:       return Unary(it, "Exp");
    case Inst::Kind::EXP2:      return Unary(it, "Exp2");
    case Inst::Kind::LOG:       return Unary(it, "Log");
    case Inst::Kind::LOG2:      return Unary(it, "Log2");
    case Inst::Kind::LOG10:     return Unary(it, "LOG10");
    case Inst::Kind::FCEIL:     return Unary(it, "Fceil");
    case Inst::Kind::FFLOOR:    return Unary(it, "Ffloor");
    case Inst::Kind::POPCNT:    return Unary(it, "Popcnt");
    case Inst::Kind::CLZ:       return Unary(it, "Clz");
    case Inst::Kind::CTZ:       return Unary(it, "Ctz");
    // Binary instructions
    case Inst::Kind::ADD:       return Binary(it, "Add");
    case Inst::Kind::AND:       return Binary(it, "And");
    case Inst::Kind::CMP:       return Binary(it, "Cmp");
    case Inst::Kind::UDIV:      return Binary(it, "Udiv");
    case Inst::Kind::UREM:      return Binary(it, "Urem");
    case Inst::Kind::SDIV:      return Binary(it, "Sdiv");
    case Inst::Kind::SREM:      return Binary(it, "Srem");
    case Inst::Kind::MUL:       return Binary(it, "Mul");
    case Inst::Kind::OR:        return Binary(it, "Or");
    case Inst::Kind::ROTL:      return Binary(it, "Rotl");
    case Inst::Kind::ROTR:      return Binary(it, "Rotr");
    case Inst::Kind::SLL:       return Binary(it, "Sll");
    case Inst::Kind::SRA:       return Binary(it, "Sra");
    case Inst::Kind::SRL:       return Binary(it, "Srl");
    case Inst::Kind::SUB:       return Binary(it, "Sub");
    case Inst::Kind::XOR:       return Binary(it, "Xor");
    case Inst::Kind::POW:       return Binary(it, "Pow");
    case Inst::Kind::COPYSIGN:  return Binary(it, "Copysign");
    case Inst::Kind::UADDO:     return Binary(it, "Uaddo");
    case Inst::Kind::UMULO:     return Binary(it, "Umulo");
    case Inst::Kind::USUBO:     return Binary(it, "Usubo");
    case Inst::Kind::SADDO:     return Binary(it, "Saddo");
    case Inst::Kind::SMULO:     return Binary(it, "Smulo");
    case Inst::Kind::SSUBO:     return Binary(it, "Ssubo");
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void CoqEmitter::Unary(Block::const_iterator it, const char *op)
{
  auto &unary = static_cast<const UnaryInst &>(*it);

  os_ << "LLUnop LL" << op << " ";
  os_ << insts_[unary.GetArg()] << "%positive ";
  os_ << insts_[&unary] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Binary(Block::const_iterator it, const char *op)
{
  os_ << "LLJmp xH";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Mov(Block::const_iterator it)
{
  os_ << "LLJmp xH";
}
