// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
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
  os_ << "Require Import LLIR.Values.\n";
  os_ << "Require Import LLIR.Verify.\n";
  os_ << "Require Import LLIR.State.\n";
  os_ << "Require Import LLIR.Export.\n";
  os_ << "Require Import LLIR.Dom.\n";
  os_ << "Require Import Coq.Lists.List.\n";
  os_ << "Import ListNotations.\n";

  os_ << "\n";
  for (const Func &func : prog) {
    WriteDefinition(func);
    WriteInversion(func);
    WriteDefinedAtInversion(func);
    WriteUsedAtInversion(func);
    WriteBlocks(func);
    WriteDominators(func);
    WriteDefsAreUniqe(func);
    WriteUsesHaveDefs(func);

    insts_.clear();
    blocks_.clear();
  }
}

// -----------------------------------------------------------------------------
std::string CoqEmitter::Name(const Func &func)
{
  std::string name;
  for (char chr : func.GetName()) {
    if (chr == '$') {
      name += "__";
    } else {
      name += chr;
    }
  }
  return name;
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDefinition(const Func &func)
{
  os_ << "Definition " << Name(func) << ": func := \n";

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
    case Inst::Kind::CALL: llvm_unreachable("CALL");
    case Inst::Kind::TCALL: llvm_unreachable("TCALL");
    case Inst::Kind::INVOKE: llvm_unreachable("INVOKE");
    case Inst::Kind::TINVOKE: llvm_unreachable("TINVOKE");
    case Inst::Kind::RET: {
      auto &inst = static_cast<const ReturnInst &>(*it);
      if (auto *val = inst.GetValue()) {
        os_ << "LLRet " << insts_[val] << "%positive";
      } else {
        os_ << "LLRetVoid";
      }
      return;
    }
    case Inst::Kind::JCC: {
      auto &inst = static_cast<const JumpCondInst &>(*it);
      os_ << "LLJcc ";
      os_ << insts_[inst.GetCond()] << "%positive ";
      os_ << blocks_[inst.GetTrueTarget()] << "%positive ";
      os_ << blocks_[inst.GetFalseTarget()] << "%positive";
      return;
    }
    case Inst::Kind::JI: llvm_unreachable("JI");
    case Inst::Kind::JMP: {
      auto &inst = static_cast<const JumpInst &>(*it);
      os_ << "LLJmp " << blocks_[inst.GetTarget()] << "%positive";
      return;
    }
    case Inst::Kind::SWITCH: llvm_unreachable("SWITCH");
    case Inst::Kind::TRAP: llvm_unreachable("TRAP");
    // Load.
    case Inst::Kind::LD: llvm_unreachable("LD");
    // Store.
    case Inst::Kind::ST: llvm_unreachable("ST");
    // Atomic exchange.
    case Inst::Kind::XCHG: llvm_unreachable("XCHG");
    // Atomic compare and exchange.
    case Inst::Kind::CMPXCHG: llvm_unreachable("CMPXCHG");
    // Set register.
    case Inst::Kind::SET: llvm_unreachable("SET");
    // Variable argument lists.
    case Inst::Kind::VASTART: llvm_unreachable("VASTART");
    // Dynamic stack allcoation.
    case Inst::Kind::ALLOCA: llvm_unreachable("ALLOCA");
    // Argument.
    case Inst::Kind::ARG: {
      auto &inst = static_cast<const ArgInst &>(*it);
      os_ << "LLArg " << inst.GetIdx() << " ";
      os_ << insts_[&inst] << "%positive ";
      os_ << insts_[&*std::next(it)] << "%positive";
      return;
    }
    // Frame address.
    case Inst::Kind::FRAME: llvm_unreachable("FRAME");
    // Undefined value.
    case Inst::Kind::UNDEF: {
      auto &inst = static_cast<const UndefInst &>(*it);
      os_ << "LLUndef ";
      Write(inst.GetType());
      os_ << " ";
      os_ << insts_[&inst] << "%positive ";
      os_ << insts_[&*std::next(it)] << "%positive";
      return;
    }
    // Hardware instructions.
    case Inst::Kind::RDTSC: llvm_unreachable("RDTSC");
    case Inst::Kind::FNSTCW: llvm_unreachable("FNSTCW");
    case Inst::Kind::FLDCW: llvm_unreachable("FLDCW");
    case Inst::Kind::SYSCALL: llvm_unreachable("SYSCALL");
    // Conditional.
    case Inst::Kind::SELECT: llvm_unreachable("SELECT");
    // PHI node.
    case Inst::Kind::PHI: llvm_unreachable("PHI");
    case Inst::Kind::MOV:       return Mov(it);
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
    case Inst::Kind::UDIV:      return Binary(it, "UDiv");
    case Inst::Kind::UREM:      return Binary(it, "URem");
    case Inst::Kind::SDIV:      return Binary(it, "SDiv");
    case Inst::Kind::SREM:      return Binary(it, "SRem");
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
    case Inst::Kind::UADDO:     return Binary(it, "UAddO");
    case Inst::Kind::UMULO:     return Binary(it, "UMulO");
    case Inst::Kind::USUBO:     return Binary(it, "USubO");
    case Inst::Kind::SADDO:     return Binary(it, "SAddO");
    case Inst::Kind::SMULO:     return Binary(it, "SMulO");
    case Inst::Kind::SSUBO:     return Binary(it, "SSubO");
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void CoqEmitter::Unary(Block::const_iterator it, const char *op)
{
  auto &unary = static_cast<const UnaryInst &>(*it);

  os_ << "LLUnop ";
  Write(unary.GetType());
  os_ << " LL" << op << " ";
  os_ << insts_[unary.GetArg()] << "%positive ";
  os_ << insts_[&unary] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Binary(Block::const_iterator it, const char *op)
{
  auto &binary = static_cast<const BinaryInst &>(*it);

  os_ << "LLBinop ";
  Write(binary.GetType());
  os_ << " LL" << op << " ";
  os_ << insts_[binary.GetLHS()] << "%positive ";
  os_ << insts_[binary.GetRHS()] << "%positive ";
  os_ << insts_[&binary] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
template<unsigned Bits>
void Int(llvm::raw_ostream &os, const APInt &val)
{
  constexpr unsigned Half = Bits / 2;
  const APInt &lo = val.extractBits(Half, 0);
  const APInt &hi = val.extractBits(Half, Half);
  os << "(";
  Int<Half>(os, hi);
  os << ", ";
  Int<Half>(os, lo);
  os << ")";
}

template<>
void Int<1>(llvm::raw_ostream &os, const APInt &val)
{
  os << (val.isNullValue() ? "O" : "I");
}

// -----------------------------------------------------------------------------
template<unsigned Bits>
void CoqEmitter::MovInt(
    Block::const_iterator it,
    const char *op,
    const APInt &val)
{
  os_ << op << " ";
  Int<Bits>(os_, val);
  os_ << " ";
  os_ << insts_[&*it] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Mov(Block::const_iterator it)
{
  auto &inst = static_cast<const MovInst &>(*it);
  auto *arg = inst.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: llvm_unreachable("INST");
    case Value::Kind::GLOBAL: llvm_unreachable("GLOBAL");
    case Value::Kind::EXPR: llvm_unreachable("EXPR");
    case Value::Kind::CONST: {
      switch (static_cast<const Constant *>(arg)->GetKind()) {
        case Constant::Kind::INT: {
          const APInt &val = static_cast<const ConstantInt *>(arg)->GetValue();
          switch (inst.GetType()) {
            case Type::I8: return MovInt<8>(it, "LLInt8", val);
            case Type::I16: return MovInt<16>(it, "LLInt16", val);
            case Type::I32: return MovInt<32>(it, "LLInt32", val);
            case Type::I64: return MovInt<64>(it, "LLInt64", val);
            case Type::I128: return MovInt<128>(it, "LLInt128", val);

            case Type::F32:
            case Type::F64:
            case Type::F80: {
              llvm_unreachable("FLOAT");
            }
          }
          llvm_unreachable("invalid instruction type");
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("FLOAT");
        }
        case Constant::Kind::REG: {
          llvm_unreachable("REG");
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteInversion(const Func &func)
{
  {
    os_ << "Theorem " << Name(func) << "_inst_inversion:\n";
    os_.indent(2) << "forall (inst: option inst) (n: node),\n";
    os_.indent(2) << "inst = (fn_insts " << Name(func) << ") ! n ->\n";
    for (const Block &block : func) {
      for (auto it = block.begin(), end = block.end(); it != end; ++it) {
        if (it->Is(Inst::Kind::PHI)) {
          continue;
        }
        os_.indent(4);
        os_ << "(" << insts_[&*it] << "%positive = n /\\ Some (";
        Write(it);
        os_ << ") = inst)\n";
        os_.indent(4);
        os_ << "\\/\n";
      }
    }
    os_.indent(4) << "inst = None.\n";
    os_ << "Proof. inst_inversion_proof " << Name(func) << ". Qed.\n\n";
  }

  {
    os_ << "Theorem " << Name(func) << "_phi_inversion:\n";
    os_.indent(2) << "forall (phis: option (list phi)) (n: node),\n";
    os_.indent(2) << "phis = (fn_phis " << Name(func) << ") ! n ->\n";
    for (const Block &block : func) {
      const auto phis = block.phis();
      if (phis.begin() == phis.end()) {
        continue;
      }
      os_.indent(4);
      os_ << "(" << blocks_[&block] << "%positive = n /\\ Some [";

      for (auto it = phis.begin(); it != phis.end(); ++it) {
        if (it != phis.begin()) {
          os_ << "; ";
        }

        os_ << "LLPhi [ ";
        for (unsigned j = 0, m = it->GetNumIncoming(); j < m; ++j) {
          if (j != 0) {
            os_ << "; ";
          }

          const Block *block = it->GetBlock(j);
          const Inst *value = it->GetValue(j);
          os_ << "(";
          os_ << insts_[block->GetTerminator()] << "%positive";
          os_ << ", ";
          os_ << insts_[value] << "%positive";
          os_ << ")";
        }
        os_ << "] " << insts_[&*it] << "%positive";
      }
      os_ << "] = phis)\n";
      os_.indent(4);
      os_ << "\\/\n";
    }
    os_.indent(4) << "phis = None.\n";
    os_ << "Proof. phi_inversion_proof " << Name(func) << ". Qed.\n\n";
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDefinedAtInversion(const Func &func)
{
  std::vector<const Inst *> insts;
  std::vector<const PhiInst *> phis;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      if (auto *phi = ::dyn_cast_or_null<const PhiInst>(&inst)) {
        phis.push_back(phi);
      } else if (inst.GetNumRets() > 0) {
        insts.push_back(&inst);
      }
    }
  }

  {
    os_ << "Theorem " << Name(func) << "_defined_at_inversion:\n";
    os_.indent(2) << "forall (n: node) (r: reg),\n";
    os_.indent(4) << "DefinedAt " << Name(func) << " n r -> \n";

    for (unsigned i = 0, n = insts.size(); i < n; ++i) {
      if (i != 0) {
        os_ << "\n"; os_.indent(6) << "\\/\n";
      }
      unsigned reg = insts_[insts[i]];
      os_.indent(6);
      os_ << "(" << reg << "%positive = n /\\ " << reg << "%positive = r)";
    }
    if (!phis.empty()) {
      for (unsigned i = 0, n = phis.size(); i < n; ++i) {
        os_ << "\n"; os_.indent(6) << "\\/\n";
        unsigned block = blocks_[phis[i]->getParent()];
        unsigned reg = insts_[phis[i]];
        os_.indent(6);
        os_ << "(" << block << "%positive = n /\\ " << reg << "%positive = r)";
      }
    }
    os_ << ".\n";
    os_ << "Proof. defined_at_inversion_proof ";
    os_ << Name(func) << " ";
    os_ << Name(func) << "_inst_inversion ";
    os_ << Name(func) << "_phi_inversion. ";
    os_ << "Qed.\n\n";
  }

  {
    os_ << "Theorem " << Name(func) << "_defined_at:\n";

    for (unsigned i = 0, n = insts.size(); i < n; ++i) {
      if (i != 0) {
        os_ << "\n"; os_.indent(2) << "/\\\n";
      }
      unsigned reg = insts_[insts[i]];
      os_.indent(2) << "DefinedAt " << Name(func) << " ";
      os_ << reg << "%positive " << reg << "%positive";
    }
    if (!phis.empty()) {
      for (unsigned i = 0, n = phis.size(); i < n; ++i) {
        os_ << "\n"; os_.indent(2) << "/\\\n";
        unsigned block = blocks_[phis[i]->getParent()];
        unsigned reg = insts_[phis[i]];
        os_.indent(2) << "DefinedAt " << Name(func) << " ";
        os_ << block << "%positive ";
        os_ << reg << "%positive";
      }
    }
    os_ << ".\n";
    os_ << "Proof. defined_at_proof " << Name(func) << ". Qed.\n\n";
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDefsAreUniqe(const Func &func)
{
  os_ << "Theorem " << Name(func) << "_defs_are_unique: ";
  os_ << "defs_are_unique " << Name(func) << ".\n";
  os_ << "Proof. ";
  os_ << "defs_are_unique_proof " << Name(func) << "_defined_at_inversion. ";
  os_ << "Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteUsesHaveDefs(const Func &func)
{
  os_ << "Theorem " << Name(func) << "_uses_have_defs: ";
  os_ << "uses_have_defs " << Name(func) << ".\n";
  os_ << "Proof. uses_have_defs_proof ";
  os_ << Name(func) << " ";
  os_ << Name(func) << "_used_at_inversion ";
  os_ << Name(func) << "_defined_at ";
  os_ << Name(func) << "_bb ";
  os_ << Name(func) << "_bb_headers_inversion ";
  os_ << Name(func) << "_dominator_solution ";
  os_ << Name(func) << "_dominator_solution_correct. ";
  os_ << "Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteUsedAtInversion(const Func &func)
{
  os_ << "Theorem " << Name(func) << "_used_at_inversion:\n";
  os_.indent(2) << "forall (n: node) (r: reg),\n";
  os_.indent(4) << "UsedAt " << Name(func) << " n r -> \n";

  std::vector<std::pair<const Inst *, const Inst *>> usedAt;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      if (auto *phi = ::dyn_cast_or_null<const PhiInst>(&inst)) {
        for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
          usedAt.emplace_back(
              phi->GetBlock(i)->GetTerminator(),
              phi->GetValue(i)
          );
        }
      } else {
        for (const Value *val : inst.operand_values()) {
          if (auto *used = ::dyn_cast_or_null<const Inst>(val)) {
            usedAt.emplace_back(&inst, used);
          }
        }
      }
    }
  }

  for (unsigned i = 0, m = usedAt.size(); i < m; ++i) {
    if (i != 0) {
      os_ << "\n"; os_.indent(6) << "\\/\n";
    }
    auto [user, reg] = usedAt[i];
    unsigned n = insts_[user];
    unsigned r = insts_[reg];
    os_.indent(6);
    os_ << "(" << n << "%positive = n /\\ " << r << "%positive = r)";
  }
  os_ << ".\n";
  os_ << "Proof. used_at_inversion_proof ";
  os_ << Name(func) << " ";
  os_ << Name(func) << "_inst_inversion ";
  os_ << Name(func) << "_phi_inversion. ";
  os_ << "Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteBlocks(const Func &func)
{
  llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);

  std::vector<std::pair<const Inst *, const Block *>> insts;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      if (!inst.Is(Inst::Kind::PHI)) {
        insts.emplace_back(&inst, &block);
      }
    }
  }

  // Find paths from entry to all blocks.
  std::unordered_map<const Block *, std::vector<const Block *>> paths;
  {
    std::vector<const Block *> path;
    std::set<const Block *> visited;

    std::function<void(const Block *)> visit = [&] (const Block *block) {
      if (!visited.insert(block).second) {
        return;
      }
      paths[block] = path;
      path.push_back(block);
      for (const Block *succ : block->successors()) {
        visit(succ);
      }
      path.pop_back();
    };
    visit(&func.getEntryBlock());
  }

  // Build an inversion proof of block headers.
  {
    os_ << "Theorem " << Name(func) << "_bb_headers_inversion: \n";
    os_.indent(2) << "forall (header: node), \n";
    os_.indent(4) << "BasicBlockHeader " << Name(func) << " header ->";
    for (auto it = blockOrder.begin(); it != blockOrder.end(); ++it) {
      if (it != blockOrder.begin()) {
        os_ << "\n"; os_.indent(6) << "\\/";
      }
      os_ << "\n";
      os_.indent(6) << blocks_[*it] << "%positive = header";
    }
    os_ << ".\n";
    os_ << "Proof. bb_headers_inversion_proof " << Name(func);
    os_ << " " << Name(func) << "_inst_inversion. Qed.\n\n";
  }

  // Build a list of block headers.
  {
    os_ << "Theorem " << Name(func) << "_bb_headers:";
    for (auto it = blockOrder.begin(); it != blockOrder.end(); ++it) {
      if (it != blockOrder.begin()) {
        os_ << "\n"; os_.indent(6) << "/\\";
      }
      os_ << "\n";
      os_.indent(6) << "BasicBlockHeader " << Name(func) << " ";
      os_ << blocks_[*it] << "%positive";
    }
    os_ << ".\n";
    os_ << "Admitted.\n\n";
    /*
    TODO
    os_ << "Proof. bb_headers_proof " << Name(func);
    os_ << " " << Name(func) << "_inst_inversion. Qed.\n\n";
    */
  }

  // Inversion for all blocks and elements.
  {
    os_ << "Theorem " << Name(func) << "_bb_inversion: \n";
    os_.indent(2) << "forall (header: node) (elem: node),\n";
    os_.indent(4) << "BasicBlock " <<  Name(func) << " header elem ->";
    for (unsigned i = 0, n = insts.size(); i < n; ++i) {
      if (i != 0) {
        os_ << "\n"; os_.indent(6) << "\\/";
      }
      auto [inst, block] = insts[i];
      os_ << "\n";
      os_.indent(6) << blocks_[block] << "%positive = header /\\ ";
      os_ << insts_[inst] << "%positive = elem";
    }
    os_ << ".\n";
    os_ << "Proof. bb_inversion_proof " << Name(func.GetName()) << " ";
    os_ << Name(func.GetName()) << "_inst_inversion ";
    os_ << Name(func.GetName()) << "_bb_headers_inversion. ";
    os_ << "Qed.\n\n";
  }

  // Enumeration of all basic blocks.
  {
    os_ << "Theorem " << Name(func) << "_bb:";
    for (unsigned i = 0, n = insts.size(); i < n; ++i) {
      if (i != 0) {
        os_ << "\n"; os_.indent(2) << "/\\";
      }
      auto [inst, block] = insts[i];
      os_ << "\n";
      os_.indent(2) << "BasicBlock " << Name(func) << " ";
      os_ << blocks_[block] << "%positive ";
      os_ << insts_[inst] << "%positive";
    }
    os_ << ".\n";
    os_ << "Proof. bb_proof ";
    os_ << Name(func.GetName()) << " ";
    os_ << Name(func.GetName()) << "_inst_inversion ";
    os_ << Name(func.GetName()) << "_bb_headers ";
    os_ << ". Qed.\n\n";
  }

  // Inversion for basic block successors.
  {
    std::vector<std::pair<const Block *, const Block *>> succs;
    for (const Block *block : blockOrder) {
      for (const Block *succ : block->successors()) {
        succs.emplace_back(block, succ);
      }
    }
    os_ << "Theorem " << Name(func) << "_bb_succ_inversion: \n";
    os_.indent(2) << "forall (from: node) (to: node),\n";
    os_.indent(4) << "BasicBlockSucc " <<  Name(func) << " from to ->";
    for (unsigned i = 0, n = succs.size(); i < n; ++i) {
      if (i != 0) {
        os_ << "\n"; os_.indent(6) << "\\/";
      }
      auto [from, to] = succs[i];
      os_ << "\n";
      os_.indent(6) << blocks_[from] << "%positive = from /\\ ";
      os_ << blocks_[to] << "%positive = to";
    }
    os_ << ".\n";
    os_ << "Proof. bb_succ_inversion_proof ";
    os_ << Name(func.GetName()) << "_bb_headers_inversion ";
    os_ << Name(func.GetName()) << "_bb_inversion. ";
    os_ << "Qed.\n\n";
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDominators(const Func &func)
{
  DominatorTree DT(const_cast<Func &>(func));

  std::vector<std::pair<const Block *, const Block *>> doms;
  std::unordered_map<const Block *, std::vector<const Block *>> paths;
  {
    std::vector<const Block *> path;
    std::function<void(const Block *)> visit = [&](const Block *b) {
      auto *node = DT.getNode(b);
      path.push_back(b);
      paths[b] = path;
      assert(node && "missing node from dominator");
      for (auto *childNode : *node) {
        auto *child = childNode->getBlock();
        doms.emplace_back(b, child);
        visit(child);
      }
      path.pop_back();
    };
    visit(&func.getEntryBlock());
  }

  os_ << "Definition " << Name(func) << "_dominator_solution := \n";
  os_.indent(2) << "<< ";
  for (auto it = paths.begin(); it != paths.end(); ++it) {
    if (it != paths.begin()) {
      os_ << ";  ";
    }
    os_ << "(" << blocks_[it->first] << "%positive, [";
    for (unsigned i = 0, end = it->second.size(); i < end; ++i) {
      if (i != 0) {
        os_ << "; ";
      }
      os_ << blocks_[it->second[i]] << "%positive";
    }
    os_ << "])\n";
    os_.indent(2);
  }
  os_ << ">>.\n\n";

  os_ << "Theorem " << Name(func) << "_dominator_solution_correct: ";
  os_ << "dominator_solution_correct " << Name(func) << " ";
  os_ << Name(func) << "_dominator_solution.\n";
  os_ << "Proof. dominator_solution_proof ";
  os_ << Name(func) << " ";
  os_ << Name(func) << "_dominator_solution ";
  os_ << Name(func) << "_bb_headers_inversion ";
  os_ << Name(func) << "_bb_succ_inversion. ";
  os_ << "Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(Type ty)
{
  switch (ty) {
    case Type::I8:   os_ << "I8";   return;
    case Type::I16:  os_ << "I16";  return;
    case Type::I32:  os_ << "I32";  return;
    case Type::I64:  os_ << "I64";  return;
    case Type::I128: os_ << "I128"; return;
    case Type::F32:  os_ << "F32";  return;
    case Type::F64:  os_ << "F64";  return;
    case Type::F80:  os_ << "F80";  return;
  }
  llvm_unreachable("invalid type");
}
