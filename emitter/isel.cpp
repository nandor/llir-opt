// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/CodeGen/MachineFrameInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/CodeGen/TargetFrameLowering.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>

#include "emitter/isel.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/data.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/prog.h"

namespace ISD = llvm::ISD;



// -----------------------------------------------------------------------------
ISel::ISel()
  : MBB_(nullptr)
{
}

// -----------------------------------------------------------------------------
void ISel::Lower(const Inst *i)
{
  if (i->IsTerminator()) {
    HandleSuccessorPHI(i->getParent());
  }

  switch (i->GetKind()) {
    // Control flow.
    case Inst::Kind::CALL:     return LowerCall(static_cast<const CallInst *>(i));
    case Inst::Kind::TCALL:    return LowerTailCall(static_cast<const TailCallInst *>(i));
    case Inst::Kind::INVOKE:   return LowerInvoke(static_cast<const InvokeInst *>(i));
    case Inst::Kind::TINVOKE:  return LowerTailInvoke(static_cast<const TailInvokeInst *>(i));
    case Inst::Kind::SYSCALL:  return LowerSyscall(static_cast<const SyscallInst *>(i));
    case Inst::Kind::RET:      return LowerReturn(static_cast<const ReturnInst *>(i));
    case Inst::Kind::JCC:      return LowerJCC(static_cast<const JumpCondInst *>(i));
    case Inst::Kind::JI:       return LowerJI(static_cast<const JumpIndirectInst *>(i));
    case Inst::Kind::JMP:      return LowerJMP(static_cast<const JumpInst *>(i));
    case Inst::Kind::SWITCH:   return LowerSwitch(static_cast<const SwitchInst *>(i));
    case Inst::Kind::TRAP:     return LowerTrap(static_cast<const TrapInst *>(i));
    // Memory.
    case Inst::Kind::LD:       return LowerLD(static_cast<const LoadInst *>(i));
    case Inst::Kind::ST:       return LowerST(static_cast<const StoreInst *>(i));
    // Atomic exchange.
    case Inst::Kind::XCHG:     return LowerXchg(static_cast<const XchgInst *>(i));
    case Inst::Kind::CMPXCHG:  return LowerCmpXchg(static_cast<const CmpXchgInst *>(i));
    // Set register.
    case Inst::Kind::SET:      return LowerSet(static_cast<const SetInst *>(i));
    // Varargs.
    case Inst::Kind::VASTART:  return LowerVAStart(static_cast<const VAStartInst *>(i));
    // Constant.
    case Inst::Kind::FRAME:    return LowerFrame(static_cast<const FrameInst *>(i));
    // Dynamic stack allocation.
    case Inst::Kind::ALLOCA:   return LowerAlloca(static_cast<const AllocaInst *>(i));
    // Conditional.
    case Inst::Kind::SELECT:   return LowerSelect(static_cast<const SelectInst *>(i));
    // Unary instructions.
    case Inst::Kind::ABS:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FABS);
    case Inst::Kind::NEG:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FNEG);
    case Inst::Kind::SQRT:     return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FSQRT);
    case Inst::Kind::SIN:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FSIN);
    case Inst::Kind::COS:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FCOS);
    case Inst::Kind::SEXT:     return LowerSExt(static_cast<const SExtInst *>(i));
    case Inst::Kind::ZEXT:     return LowerZExt(static_cast<const ZExtInst *>(i));
    case Inst::Kind::XEXT:     return LowerXExt(static_cast<const XExtInst *>(i));
    case Inst::Kind::FEXT:     return LowerFExt(static_cast<const FExtInst *>(i));
    case Inst::Kind::MOV:      return LowerMov(static_cast<const MovInst *>(i));
    case Inst::Kind::TRUNC:    return LowerTrunc(static_cast<const TruncInst *>(i));
    case Inst::Kind::EXP:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FEXP);
    case Inst::Kind::EXP2:     return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FEXP2);
    case Inst::Kind::LOG:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FLOG);
    case Inst::Kind::LOG2:     return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FLOG2);
    case Inst::Kind::LOG10:    return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FLOG10);
    case Inst::Kind::FCEIL:    return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FCEIL);
    case Inst::Kind::FFLOOR:   return LowerUnary(static_cast<const UnaryInst *>(i), ISD::FFLOOR);
    case Inst::Kind::POPCNT:   return LowerUnary(static_cast<const UnaryInst *>(i), ISD::CTPOP);
    case Inst::Kind::CLZ:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::CTLZ);
    case Inst::Kind::CTZ:      return LowerUnary(static_cast<const UnaryInst *>(i), ISD::CTTZ);
    // Binary instructions.
    case Inst::Kind::CMP:      return LowerCmp(static_cast<const CmpInst *>(i));
    case Inst::Kind::UDIV:     return LowerBinary(i, ISD::UDIV, ISD::FDIV);
    case Inst::Kind::SDIV:     return LowerBinary(i, ISD::SDIV, ISD::FDIV);
    case Inst::Kind::UREM:     return LowerBinary(i, ISD::UREM, ISD::FREM);
    case Inst::Kind::SREM:     return LowerBinary(i, ISD::SREM, ISD::FREM);
    case Inst::Kind::MUL:      return LowerBinary(i, ISD::MUL,  ISD::FMUL);
    case Inst::Kind::ADD:      return LowerBinary(i, ISD::ADD,  ISD::FADD);
    case Inst::Kind::SUB:      return LowerBinary(i, ISD::SUB,  ISD::FSUB);
    case Inst::Kind::AND:      return LowerBinary(i, ISD::AND);
    case Inst::Kind::OR:       return LowerBinary(i, ISD::OR);
    case Inst::Kind::SLL:      return LowerBinary(i, ISD::SHL);
    case Inst::Kind::SRA:      return LowerBinary(i, ISD::SRA);
    case Inst::Kind::SRL:      return LowerBinary(i, ISD::SRL);
    case Inst::Kind::XOR:      return LowerBinary(i, ISD::XOR);
    case Inst::Kind::ROTL:     return LowerBinary(i, ISD::ROTL);
    case Inst::Kind::ROTR:     return LowerBinary(i, ISD::ROTR);
    case Inst::Kind::POW:      return LowerBinary(i, ISD::FPOW);
    case Inst::Kind::COPYSIGN: return LowerBinary(i, ISD::FCOPYSIGN);
    // Overflow checks.
    case Inst::Kind::UADDO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::UADDO);
    case Inst::Kind::UMULO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::UMULO);
    case Inst::Kind::USUBO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::USUBO);
    case Inst::Kind::SADDO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::SADDO);
    case Inst::Kind::SMULO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::SMULO);
    case Inst::Kind::SSUBO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::SSUBO);
    // Undefined value.
    case Inst::Kind::UNDEF:    return LowerUndef(static_cast<const UndefInst *>(i));
    // Hardware instructions.
    case Inst::Kind::RDTSC:    return LowerRDTSC(static_cast<const RdtscInst *>(i));
    case Inst::Kind::FNSTCW:   return LowerFNStCw(static_cast<const FNStCwInst *>(i));
    case Inst::Kind::FLDCW:    return LowerFLdCw(static_cast<const FLdCwInst *>(i));
    // Nodes handled separately.
    case Inst::Kind::PHI:      return;
    case Inst::Kind::ARG:      return;
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetValue(const Inst *inst)
{
  if (auto vt = values_.find(inst); vt != values_.end()) {
    return vt->second;
  }

  if (auto rt = regs_.find(inst); rt != regs_.end()) {
    llvm::SelectionDAG &dag = GetDAG();
    return dag.getCopyFromReg(
        dag.getEntryNode(),
        SDL_,
        rt->second,
        GetType(inst->GetType(0))
    );
  } else {
    return LowerConstant(inst);
  }
}

// -----------------------------------------------------------------------------
void ISel::Export(const Inst *inst, SDValue values)
{
  values_[inst] = values;
  auto it = regs_.find(inst);
  if (it != regs_.end()) {
    CopyToVreg(it->second, values);
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetExportRoot()
{
  llvm::SelectionDAG &dag = GetDAG();

  SDValue root = dag.getRoot();
  if (pendingExports_.empty()) {
    return root;
  }

  bool exportsRoot = false;
  llvm::SmallVector<llvm::SDValue, 8> exports;
  for (auto &exp : pendingExports_) {
    exports.push_back(dag.getCopyToReg(
        dag.getEntryNode(),
        SDL_,
        exp.first,
        exp.second
    ));

    auto *node = exp.second.getNode();
    if (node->getNumOperands() > 0 && node->getOperand(0) == root) {
      exportsRoot = true;
    }
  }

  if (root.getOpcode() != ISD::EntryToken && !exportsRoot) {
    exports.push_back(root);
  }

  SDValue factor = dag.getNode(
      ISD::TokenFactor,
      SDL_,
      MVT::Other,
      exports
  );
  dag.setRoot(factor);
  pendingExports_.clear();
  return factor;
}

// -----------------------------------------------------------------------------
void ISel::CopyToVreg(unsigned reg, llvm::SDValue value)
{
  pendingExports_.emplace(reg, value);
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerImm(const APInt &val, Type type)
{
  union U { int64_t i; double d; };
  switch (type) {
    case Type::I8:
      return GetDAG().getConstant(val.sextOrTrunc(8), SDL_, MVT::i8);
    case Type::I16:
      return GetDAG().getConstant(val.sextOrTrunc(16), SDL_, MVT::i16);
    case Type::I32:
      return GetDAG().getConstant(val.sextOrTrunc(32), SDL_, MVT::i32);
    case Type::I64:
      return GetDAG().getConstant(val.sextOrTrunc(64), SDL_, MVT::i64);
    case Type::I128:
      return GetDAG().getConstant(val.sextOrTrunc(128), SDL_, MVT::i128);
    case Type::F32: {
      U u { .i = val.getSExtValue() };
      return GetDAG().getConstantFP(u.d, SDL_, MVT::f32);
    }
    case Type::F64: {
      U u { .i = val.getSExtValue() };
      return GetDAG().getConstantFP(u.d, SDL_, MVT::f64);
    }
    case Type::F80: {
      U u { .i = val.getSExtValue() };
      return GetDAG().getConstantFP(u.d, SDL_, MVT::f80);
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerImm(const APFloat &val, Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128:
      llvm_unreachable("not supported");
    case Type::F32:
      return GetDAG().getConstantFP(val, SDL_, MVT::f32);
    case Type::F64:
      return GetDAG().getConstantFP(val, SDL_, MVT::f64);
    case Type::F80:
      return GetDAG().getConstantFP(val, SDL_, MVT::f80);
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerConstant(const Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<const MovInst>(inst)) {
    Type rt = movInst->GetType();
    switch (auto *val = movInst->GetArg(); val->GetKind()) {
      case Value::Kind::INST: {
        Error(inst, "not a constant");
      }
      case Value::Kind::CONST: {
        switch (static_cast<Constant *>(val)->GetKind()) {
          case Constant::Kind::REG: {
            Error(inst, "not a constant");
          }
          case Constant::Kind::INT: {
            return LowerImm(static_cast<ConstantInt *>(val)->GetValue(), rt);
          }
          case Constant::Kind::FLOAT: {
            return LowerImm(static_cast<ConstantFloat *>(val)->GetValue(), rt);
          }
        }
        llvm_unreachable("invalid constant kind");
      }
      case Value::Kind::GLOBAL: {
        if (!IsPointerType(movInst->GetType())) {
          Error(movInst, "Invalid address type");
        }
        return LowerGlobal(static_cast<Global *>(val), 0);
      }
      case Value::Kind::EXPR: {
        if (!IsPointerType(movInst->GetType())) {
          Error(movInst, "Invalid address type");
        }
        return LowerExpr(static_cast<const Expr *>(val));
      }
    }
    llvm_unreachable("invalid value kind");
  } else {
    Error(inst, "not a move instruction");
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerExpr(const Expr *expr)
{
  switch (expr->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto *symOff = static_cast<const SymbolOffsetExpr *>(expr);
      return LowerGlobal(symOff->GetSymbol(), symOff->GetOffset());
    }
  }
  llvm_unreachable("invalid expression");
}

// -----------------------------------------------------------------------------
llvm::MVT ISel::GetType(Type t)
{
  switch (t) {
    case Type::I8:   return MVT::i8;
    case Type::I16:  return MVT::i16;
    case Type::I32:  return MVT::i32;
    case Type::I64:  return MVT::i64;
    case Type::I128: return MVT::i128;
    case Type::F32:  return MVT::f32;
    case Type::F64:  return MVT::f64;
    case Type::F80:  return MVT::f80;
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
ISD::CondCode ISel::GetCond(Cond cc)
{
  switch (cc) {
    case Cond::EQ:  return ISD::CondCode::SETEQ;
    case Cond::NE:  return ISD::CondCode::SETNE;
    case Cond::LE:  return ISD::CondCode::SETLE;
    case Cond::LT:  return ISD::CondCode::SETLT;
    case Cond::GE:  return ISD::CondCode::SETGE;
    case Cond::GT:  return ISD::CondCode::SETGT;
    case Cond::OEQ: return ISD::CondCode::SETOEQ;
    case Cond::ONE: return ISD::CondCode::SETONE;
    case Cond::OLE: return ISD::CondCode::SETOLE;
    case Cond::OLT: return ISD::CondCode::SETOLT;
    case Cond::OGE: return ISD::CondCode::SETOGE;
    case Cond::OGT: return ISD::CondCode::SETOGT;
    case Cond::UEQ: return ISD::CondCode::SETUEQ;
    case Cond::UNE: return ISD::CondCode::SETUNE;
    case Cond::ULE: return ISD::CondCode::SETULE;
    case Cond::ULT: return ISD::CondCode::SETULT;
    case Cond::UGE: return ISD::CondCode::SETUGE;
    case Cond::UGT: return ISD::CondCode::SETUGT;
  }
  llvm_unreachable("invalid condition");
}

// -----------------------------------------------------------------------------
void ISel::BundleAnnotations(const Block *block, llvm::MachineBasicBlock *mbb)
{
  // Labels can be placed after call sites, succeeded stack adjustment
  // and spill-restore instructions. This step adjusts label positions:
  // finds the GC_FRAME, removes it and inserts it after the preceding call.
  for (auto it = mbb->rbegin(); it != mbb->rend(); it++) {
    if (it->isGCRoot() || it->isGCCall()) {
      auto jt = it;
      do { jt--; } while (!jt->isCall());
      auto *mi = it->removeFromParent();
      mbb->insertAfter(jt->getIterator(), mi);
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::HandleSuccessorPHI(const Block *block)
{
  llvm::MachineRegisterInfo &regInfo = GetRegisterInfo();
  llvm::SelectionDAG &dag = GetDAG();
  const llvm::TargetLowering &tli = GetTargetLowering();

  auto *blockMBB = blocks_[block];
  llvm::SmallPtrSet<llvm::MachineBasicBlock *, 4> handled;
  for (const Block *succBB : block->successors()) {
    llvm::MachineBasicBlock *succMBB = blocks_[succBB];
    if (!handled.insert(succMBB).second) {
      continue;
    }

    auto phiIt = succMBB->begin();
    for (const PhiInst &phi : succBB->phis()) {
      if (phi.use_empty()) {
        continue;
      }

      llvm::MachineInstrBuilder mPhi(dag.getMachineFunction(), phiIt++);
      const Inst *inst = phi.GetValue(block);
      unsigned reg = 0;
      Type phiType = phi.GetType();
      MVT VT = GetType(phiType);

      if (auto *movInst = ::dyn_cast_or_null<const MovInst>(inst)) {
        auto *arg = movInst->GetArg();
        switch (arg->GetKind()) {
          case Value::Kind::INST: {
            auto it = regs_.find(inst);
            if (it != regs_.end()) {
              reg = it->second;
            } else {
              reg = regInfo.createVirtualRegister(tli.getRegClassFor(VT));
              CopyToVreg(reg, LowerConstant(inst));
            }
            break;
          }
          case Value::Kind::GLOBAL: {
            if (!IsPointerType(phi.GetType())) {
              Error(&phi, "Invalid address type");
            }
            reg = regInfo.createVirtualRegister(tli.getRegClassFor(VT));
            CopyToVreg(reg, LowerGlobal(static_cast<const Global *>(arg), 0));
            break;
          }
          case Value::Kind::EXPR: {
            if (!IsPointerType(phi.GetType())) {
              Error(&phi, "Invalid address type");
            }
            reg = regInfo.createVirtualRegister(tli.getRegClassFor(VT));
            CopyToVreg(reg, LowerExpr(static_cast<const Expr *>(arg)));
            break;
          }
          case Value::Kind::CONST: {
            SDValue value;
            switch (static_cast<const Constant *>(arg)->GetKind()) {
              case Constant::Kind::INT: {
                value = LowerImm(
                    static_cast<const ConstantInt *>(arg)->GetValue(),
                    phiType
                );
                break;
              }
              case Constant::Kind::FLOAT: {
                value = LowerImm(
                    static_cast<const ConstantFloat *>(arg)->GetValue(),
                    phiType
                );
                break;
              }
              case Constant::Kind::REG: {
                Error(&phi, "Invalid incoming register to PHI.");
              }
            }
            reg = regInfo.createVirtualRegister(tli.getRegClassFor(VT));
            CopyToVreg(reg, value);
            break;
          }
        }
      } else {
        auto it = regs_.find(inst);
        assert(it != regs_.end() && "missing vreg value");
        reg = it->second;
      }

      mPhi.addReg(reg).addMBB(blockMBB);
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::CodeGenAndEmitDAG()
{
  bool changed;

  llvm::AAResults *aa = nullptr;
  llvm::SelectionDAG &dag = GetDAG();
  llvm::CodeGenOpt::Level ol = GetOptLevel();

  dag.NewNodesMustHaveLegalTypes = false;
  dag.Combine(llvm::BeforeLegalizeTypes, aa, ol);
  changed = dag.LegalizeTypes();
  dag.NewNodesMustHaveLegalTypes = true;

  if (changed) {
    dag.Combine(llvm::AfterLegalizeTypes, aa, ol);
  }

  changed = dag.LegalizeVectors();

  if (changed) {
    dag.LegalizeTypes();
    dag.Combine(llvm::AfterLegalizeVectorOps, aa, ol);
  }

  dag.Legalize();
  dag.Combine(llvm::AfterLegalizeDAG, aa, ol);

  DoInstructionSelection();

  llvm::ScheduleDAGSDNodes *Scheduler = CreateScheduler();
  Scheduler->Run(&dag, MBB_);

  llvm::MachineBasicBlock *Fst = MBB_;
  MBB_ = Scheduler->EmitSchedule(insert_);
  llvm::MachineBasicBlock *Snd = MBB_;

  if (Fst != Snd) {
    assert(!"not implemented");
  }
  delete Scheduler;

  dag.clear();
}

// -----------------------------------------------------------------------------
class ISelUpdater : public llvm::SelectionDAG::DAGUpdateListener {
public:
  ISelUpdater(
      llvm::SelectionDAG &dag,
      llvm::SelectionDAG::allnodes_iterator &isp)
    : llvm::SelectionDAG::DAGUpdateListener(dag)
    , it_(isp)
  {
  }

  void NodeDeleted(llvm::SDNode *n, llvm::SDNode *) override {
    if (it_ == llvm::SelectionDAG::allnodes_iterator(n)) {
      ++it_;
    }
  }

private:
  llvm::SelectionDAG::allnodes_iterator &it_;
};

// -----------------------------------------------------------------------------
void ISel::DoInstructionSelection()
{
  PreprocessISelDAG();

  llvm::SelectionDAG &dag = GetDAG();

  dag.AssignTopologicalOrder();

  llvm::HandleSDNode dummy(dag.getRoot());
  llvm::SelectionDAG::allnodes_iterator it(dag.getRoot().getNode());
  ++it;

  ISelUpdater ISU(dag, it);

  while (it != dag.allnodes_begin()) {
    SDNode *node = &*--it;
    if (node->use_empty()) {
      continue;
    }
    if (node->isStrictFPOpcode()) {
      node = dag.mutateStrictFPToFP(node);
    }

    Select(node);
  }

  dag.setRoot(dummy.getValue());
}

// -----------------------------------------------------------------------------
[[noreturn]] void ISel::Error(const Inst *i, const std::string_view &message)
{
  auto block = i->getParent();
  auto func = block->getParent();

  std::ostringstream os;
  os << func->GetName() << "," << block->GetName() << ": " << message;
  llvm::report_fatal_error(os.str());
}

// -----------------------------------------------------------------------------
[[noreturn]] void ISel::Error(const Func *f, const std::string_view &message)
{
  std::ostringstream os;
  os << f->GetName() << ": " << message;
  llvm::report_fatal_error(os.str());
}

// -----------------------------------------------------------------------------
void ISel::LowerBinary(const Inst *inst, unsigned op)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);

  MVT type = GetType(binaryInst->GetType());
  SDValue lhs = GetValue(binaryInst->GetLHS());
  SDValue rhs = GetValue(binaryInst->GetRHS());
  SDValue binary = GetDAG().getNode(op, SDL_, type, lhs, rhs);
  Export(inst, binary);
}

// -----------------------------------------------------------------------------
void ISel::LowerBinary(const Inst *inst, unsigned iop, unsigned fop)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);
  switch (binaryInst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      LowerBinary(inst, iop);
      break;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80: {
      LowerBinary(inst, fop);
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerUnary(const UnaryInst *inst, unsigned op)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  SDValue arg = GetValue(inst->GetArg());
  SDValue unary = GetDAG().getNode(op, SDL_, GetType(retTy), arg);
  Export(inst, unary);
}

// -----------------------------------------------------------------------------
void ISel::LowerJCC(const JumpCondInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  auto *sourceMBB = blocks_[inst->getParent()];
  auto *trueMBB = blocks_[inst->GetTrueTarget()];
  auto *falseMBB = blocks_[inst->GetFalseTarget()];

  Inst *condInst = inst->GetCond();

  if (trueMBB == falseMBB) {
    dag.setRoot(dag.getNode(
        ISD::BR,
        SDL_,
        MVT::Other,
        GetExportRoot(),
        dag.getBasicBlock(trueMBB)
    ));

    sourceMBB->addSuccessor(trueMBB);
  } else {
    SDValue chain = GetExportRoot();
    SDValue cond = GetValue(condInst);

    cond = dag.getSetCC(
        SDL_,
        MVT::i8,
        cond,
        dag.getConstant(0, SDL_, GetType(condInst->GetType(0))),
        ISD::CondCode::SETNE
    );

    chain = dag.getNode(
        ISD::BRCOND,
        SDL_,
        MVT::Other,
        chain,
        cond,
        dag.getBasicBlock(blocks_[inst->GetTrueTarget()])
    );

    chain = dag.getNode(
        ISD::BR,
        SDL_,
        MVT::Other,
        chain,
        dag.getBasicBlock(blocks_[inst->GetFalseTarget()])
    );

    dag.setRoot(chain);

    sourceMBB->addSuccessorWithoutProb(trueMBB);
    sourceMBB->addSuccessorWithoutProb(falseMBB);
  }
  sourceMBB->normalizeSuccProbs();
}

// -----------------------------------------------------------------------------
void ISel::LowerJI(const JumpIndirectInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  auto target = inst->GetTarget();
  if (!IsPointerType(target->GetType(0))) {
    Error(inst, "invalid jump target");
  }

  dag.setRoot(dag.getNode(
      ISD::BRIND,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      GetValue(target)
  ));
}

// -----------------------------------------------------------------------------
void ISel::LowerJMP(const JumpInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  Block *target = inst->getSuccessor(0);
  auto *sourceMBB = blocks_[inst->getParent()];
  auto *targetMBB = blocks_[target];

  dag.setRoot(dag.getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      dag.getBasicBlock(targetMBB)
  ));

  sourceMBB->addSuccessor(targetMBB);
}

// -----------------------------------------------------------------------------
void ISel::LowerLD(const LoadInst *ld)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type type = ld->GetType();

  SDValue l = dag.getLoad(
      GetType(type),
      SDL_,
      dag.getRoot(),
      GetValue(ld->GetAddr()),
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      GetAlignment(type),
      llvm::MachineMemOperand::MONone,
      llvm::AAMDNodes(),
      nullptr
  );

  dag.setRoot(l.getValue(1));
  Export(ld, l);
}

// -----------------------------------------------------------------------------
void ISel::LowerST(const StoreInst *st)
{
  llvm::SelectionDAG &dag = GetDAG();

  Inst *val = st->GetVal();
  Type type = val->GetType(0);

  dag.setRoot(dag.getStore(
      dag.getRoot(),
      SDL_,
      GetValue(val),
      GetValue(st->GetAddr()),
      llvm::MachinePointerInfo(0u),
      GetAlignment(type),
      llvm::MachineMemOperand::MONone,
      llvm::AAMDNodes()
  ));
}

// -----------------------------------------------------------------------------
void ISel::LowerFrame(const FrameInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  if (auto It = stackIndices_.find(inst->GetObject()); It != stackIndices_.end()) {
    SDValue base = dag.getFrameIndex(It->second, MVT::i64);
    if (auto offest = inst->GetOffset()) {
      Export(inst, dag.getNode(
          ISD::ADD,
          SDL_,
          MVT::i64,
          base,
          dag.getConstant(offest, SDL_, MVT::i64)
      ));
    } else {
      Export(inst, base);
    }
    return;
  }
  Error(inst, "invalid frame index");
}

// -----------------------------------------------------------------------------
void ISel::LowerCmp(const CmpInst *cmpInst)
{
  llvm::SelectionDAG &dag = GetDAG();

  MVT type = GetType(cmpInst->GetType());
  SDValue lhs = GetValue(cmpInst->GetLHS());
  SDValue rhs = GetValue(cmpInst->GetRHS());
  ISD::CondCode cc = GetCond(cmpInst->GetCC());
  SDValue flag = dag.getSetCC(SDL_, MVT::i8, lhs, rhs, cc);
  if (type != MVT::i8) {
    flag = dag.getZExtOrTrunc(flag, SDL_, type);
  }
  Export(cmpInst, flag);
}

// -----------------------------------------------------------------------------
void ISel::LowerTrap(const TrapInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();
  dag.setRoot(dag.getNode(ISD::TRAP, SDL_, MVT::Other, dag.getRoot()));
}

// -----------------------------------------------------------------------------
void ISel::LowerMov(const MovInst *inst)
{
  Type retType = inst->GetType();

  auto *val = inst->GetArg();
  switch (val->GetKind()) {
    case Value::Kind::INST: {
      auto *arg = static_cast<Inst *>(val);
      SDValue argNode = GetValue(arg);
      Type argType = arg->GetType(0);
      if (argType == retType) {
        Export(inst, argNode);
      } else if (GetSize(argType) == GetSize(retType)) {
        Export(inst, GetDAG().getBitcast(GetType(retType), argNode));
      } else {
        Error(inst, "unsupported mov");
      }
      break;
    }
    case Value::Kind::CONST: {
      switch (static_cast<Constant *>(val)->GetKind()) {
        case Constant::Kind::REG: {
          Export(inst, LoadReg(static_cast<ConstantReg *>(val)->GetValue()));
          break;
        }
        case Constant::Kind::INT:
        case Constant::Kind::FLOAT:
          break;
      }
    }
    case Value::Kind::GLOBAL:
    case Value::Kind::EXPR: {
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerSExt(const SExtInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();
  MVT retMVT = GetType(retTy);
  SDValue arg = GetValue(inst->GetArg());

  if (IsIntegerType(argTy)) {
    unsigned opcode;
    if (IsIntegerType(retTy)) {
      opcode = ISD::SIGN_EXTEND;
    } else {
      opcode = ISD::SINT_TO_FP;
    }

    Export(inst, dag.getNode(opcode, SDL_, retMVT, arg));
  } else {
    if (IsIntegerType(retTy)) {
      Export(inst, dag.getNode(ISD::FP_TO_SINT, SDL_, retMVT, arg));
    } else {
      Error(inst, "invalid sext: float -> float");
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerZExt(const ZExtInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();
  MVT retMVT = GetType(retTy);
  SDValue arg = GetValue(inst->GetArg());

  if (IsIntegerType(argTy)) {
    unsigned opcode;
    if (IsIntegerType(retTy)) {
      opcode = ISD::ZERO_EXTEND;
    } else {
      opcode = ISD::UINT_TO_FP;
    }

    Export(inst, dag.getNode(opcode, SDL_, retMVT, arg));
  } else {
    if (IsIntegerType(retTy)) {
      Export(inst, dag.getNode(ISD::FP_TO_UINT, SDL_, retMVT, arg));
    } else {
      Error(inst, "invalid zext: float -> float");
    }
  }
}
// -----------------------------------------------------------------------------
void ISel::LowerXExt(const XExtInst *inst)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();
  MVT retMVT = GetType(retTy);
  SDValue arg = GetValue(inst->GetArg());

  if (IsIntegerType(argTy)) {
    if (IsIntegerType(retTy)) {
      Export(inst, GetDAG().getNode(ISD::ANY_EXTEND, SDL_, retMVT, arg));
    } else {
      Error(inst, "invalid xext to float");
    }
  } else {
    Error(inst, "invalid xext from float");
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerFExt(const FExtInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  if (!IsFloatType(argTy) || !IsFloatType(retTy)) {
    Error(inst, "argument/return not a float");
  }
  if (GetSize(argTy) >= GetSize(retTy)) {
    Error(inst, "Cannot shrink argument");
  }

  SDValue arg = GetValue(inst->GetArg());

  if (argTy == Type::F80 || retTy == Type::F80) {
    MVT argType = GetType(argTy);
    MVT retType = GetType(retTy);
    SDValue stackTmp = dag.CreateStackTemporary(argType);
    SDValue store = dag.getTruncStore(
        dag.getEntryNode(),
        SDL_,
        arg,
        stackTmp,
        llvm::MachinePointerInfo(),
        argType
    );
    Export(inst, dag.getExtLoad(
        ISD::EXTLOAD,
        SDL_,
        retType,
        store,
        stackTmp,
        llvm::MachinePointerInfo(),
        retType
    ));
  } else {
    SDValue fext = dag.getNode(ISD::FP_EXTEND, SDL_, GetType(retTy), arg);
    Export(inst, fext);
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerTrunc(const TruncInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  MVT retMVT = GetType(retTy);
  SDValue arg = GetValue(inst->GetArg());

  unsigned opcode;
  if (IsFloatType(retTy)) {
    if (IsIntegerType(argTy)) {
      Error(inst, "Cannot truncate int -> float");
    } else {
      if (argTy == Type::F80 || retTy == Type::F80) {
        SDValue stackTmp = dag.CreateStackTemporary(retMVT);
        SDValue store = dag.getTruncStore(
            dag.getEntryNode(),
            SDL_,
            arg,
            stackTmp,
            llvm::MachinePointerInfo(),
            retMVT
        );
        Export(inst, dag.getExtLoad(
            ISD::EXTLOAD,
            SDL_,
            retMVT,
            store,
            stackTmp,
            llvm::MachinePointerInfo(),
            retMVT
        ));
      } else {
        Export(inst, dag.getNode(
            ISD::FP_ROUND,
            SDL_,
            retMVT,
            arg,
            dag.getTargetConstant(0, SDL_, GetPtrTy())
        ));
      }
    }
  } else {
    if (IsIntegerType(argTy)) {
      Export(inst, dag.getNode(ISD::TRUNCATE, SDL_, retMVT, arg));
    } else {
      Export(inst, dag.getNode(ISD::FP_TO_SINT, SDL_, retMVT, arg));
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerXchg(const XchgInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  auto *mmo = dag.getMachineFunction().getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad |
      llvm::MachineMemOperand::MOStore,
      GetSize(inst->GetType()),
      llvm::Align(GetSize(inst->GetType())),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue xchg = dag.getAtomic(
      ISD::ATOMIC_SWAP,
      SDL_,
      GetType(inst->GetType()),
      dag.getRoot(),
      GetValue(inst->GetAddr()),
      GetValue(inst->GetVal()),
      mmo
  );

  dag.setRoot(xchg.getValue(1));
  Export(inst, xchg.getValue(0));
}

// -----------------------------------------------------------------------------
void ISel::LowerAlloca(const AllocaInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();
  llvm::MachineFunction &mf = dag.getMachineFunction();

  // Get the inputs.
  unsigned Align = inst->GetAlign();
  SDValue Size = GetValue(inst->GetCount());
  EVT VT = GetType(inst->GetType());
  SDValue Chain = dag.getRoot();

  // Create a chain for unique ordering.
  Chain = dag.getCALLSEQ_START(Chain, 0, 0, SDL_);

  const llvm::TargetLowering &TLI = dag.getTargetLoweringInfo();
  unsigned SPReg = TLI.getStackPointerRegisterToSaveRestore();
  assert(SPReg && "Cannot find stack pointer");

  SDValue SP = dag.getCopyFromReg(Chain, SDL_, SPReg, VT);
  Chain = SP.getValue(1);

  // Adjust the stack pointer.
  SDValue Result = dag.getNode(ISD::SUB, SDL_, VT, SP, Size);
  if (Align > mf.getSubtarget().getFrameLowering()->getStackAlignment()) {
    Result = dag.getNode(
        ISD::AND,
        SDL_,
        VT,
        Result,
        dag.getConstant(-(uint64_t)Align, SDL_, VT)
    );
  }
  Chain = dag.getCopyToReg(Chain, SDL_, SPReg, Result);

  Chain = dag.getCALLSEQ_END(
      Chain,
      dag.getIntPtrConstant(0, SDL_, true),
      dag.getIntPtrConstant(0, SDL_, true),
      SDValue(),
      SDL_
  );

  dag.setRoot(Chain);
  Export(inst, Result);

  mf.getFrameInfo().setHasVarSizedObjects(true);
}

// -----------------------------------------------------------------------------
void ISel::LowerSelect(const SelectInst *select)
{
  SDValue node = GetDAG().getNode(
      ISD::SELECT,
      SDL_,
      GetType(select->GetType()),
      GetValue(select->GetCond()),
      GetValue(select->GetTrue()),
      GetValue(select->GetFalse())
  );
  Export(select, node);
}

// -----------------------------------------------------------------------------
void ISel::LowerUndef(const UndefInst *inst)
{
  Export(inst, GetDAG().getUNDEF(GetType(inst->GetType())));
}

// -----------------------------------------------------------------------------
void ISel::LowerALUO(const OverflowInst *inst, unsigned op)
{
  llvm::SelectionDAG &dag = GetDAG();

  MVT retType = GetType(inst->GetType(0));
  MVT type = GetType(inst->GetLHS()->GetType(0));
  SDValue lhs = GetValue(inst->GetLHS());
  SDValue rhs = GetValue(inst->GetRHS());

  SDVTList types = dag.getVTList(type, MVT::i1);
  SDValue node = dag.getNode(op, SDL_, types, lhs, rhs);
  SDValue flag = dag.getZExtOrTrunc(node.getValue(1), SDL_, retType);

  Export(inst, flag);
}
