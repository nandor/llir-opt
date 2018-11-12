// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>

#include "core/block.h"
#include "core/context.h"
#include "core/data.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/symbol.h"
#include "emitter/x86/x86isel.h"

namespace ISD = llvm::ISD;
namespace X86 = llvm::X86;
namespace X86ISD = llvm::X86ISD;
using MVT = llvm::MVT;
using SDNodeFlags = llvm::SDNodeFlags;
using SDNode = llvm::SDNode;
using SDValue = llvm::SDValue;
using SelectionDAG = llvm::SelectionDAG;



// -----------------------------------------------------------------------------
char X86ISel::ID;

// -----------------------------------------------------------------------------
X86ISel::X86ISel(
    llvm::X86TargetMachine *TM,
    llvm::X86Subtarget *STI,
    const llvm::X86InstrInfo *TII,
    const llvm::X86RegisterInfo *TRI,
    const llvm::TargetLowering *TLI,
    llvm::TargetLibraryInfo *LibInfo,
    const Prog *prog,
    llvm::CodeGenOpt::Level OL)
  : X86DAGMatcher(*TM, OL, STI)
  , DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , ModulePass(ID)
  , TRI_(TRI)
  , LibInfo_(LibInfo)
  , prog_(prog)
  , MBB_(nullptr)
{
}

// -----------------------------------------------------------------------------
static bool IsExported(const Inst *inst) {
  if (inst->use_empty()) {
    return false;
  }
  if (inst->Is(Inst::Kind::PHI)) {
    return true;
  }
  const Block *parent = inst->GetParent();
  for (const User *user : inst->users()) {
    auto *value = static_cast<const Inst *>(user);
    if (value->GetParent() != parent || value->Is(Inst::Kind::PHI)) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool X86ISel::runOnModule(llvm::Module &Module)
{
  M = &Module;

  auto &Ctx = M->getContext();
  voidTy_ = llvm::Type::getVoidTy(Ctx);
  funcTy_ = llvm::FunctionType::get(voidTy_, {});

  // Populate the symbol table.
  for (const Func &func : *prog_) {
     M->getOrInsertFunction(func.GetName().data(), funcTy_);
  }
  if (auto *data = prog_->GetData()) {
    LowerData(data);
  }
  if (auto *bss = prog_->GetBSS()) {
    LowerData(bss);
  }
  if (auto *cst = prog_->GetConst()) {
    LowerData(cst);
  }

  // Generate code for functions.
  auto &MMI = getAnalysis<llvm::MachineModuleInfo>();
  for (const Func &func : *prog_) {
    // Empty function - skip it.
    if (func.IsEmpty()) {
      continue;
    }

    // Create a new dummy empty Function.
    // The IR function simply returns void since it cannot be empty.
    auto *C = M->getOrInsertFunction(func.GetName().data(), funcTy_);
    auto *F = llvm::dyn_cast<llvm::Function>(C);
    llvm::BasicBlock* block = llvm::BasicBlock::Create(F->getContext(), "entry", F);
    llvm::IRBuilder<> builder(block);
    builder.CreateRetVoid();

    // Create a MachineFunction, attached to the dummy one.
    auto ORE = std::make_unique<llvm::OptimizationRemarkEmitter>(F);
    MF = &MMI.getOrCreateMachineFunction(*F);

    // Initialise the dag with info for this function.
    CurDAG->init(*MF, *ORE, this, LibInfo_, nullptr);

    // Traverse nodes, entry first.
    llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);

    // Create a MBB for all GenM blocks, isolating the entry block.
    llvm::MachineBasicBlock *entry = nullptr;
    auto *RegInfo = &MF->getRegInfo();
    for (const auto &block : blockOrder) {
      llvm::MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(nullptr);
      blocks_[block] = MBB;

      // Create nodes for all PHIs.
      for (const PhiInst &phi : block->phis()) {
        MVT VT = GetType(phi.GetType());
        unsigned reg = RegInfo->createVirtualRegister(TLI->getRegClassFor(VT));
        BuildMI(MBB, DL_, TII->get(llvm::TargetOpcode::PHI), reg);
        regs_[&phi] = reg;
      }

      entry = entry ? entry : MBB;
    }

    // Lower individual blocks.
    for (const Block *block : blockOrder) {
      MBB_ = blocks_[block];
      MF->push_back(MBB_);

      // Set up the SelectionDAG for the block.
      Chain = CurDAG->getRoot();
      for (const auto &inst : *block) {
        Lower(&inst);
      }
      CurDAG->setRoot(Chain);

      // Lower the block.
      insert_ = MBB_->end();
      CodeGenAndEmitDAG();

      // Clear values, except exported ones.
      values_.clear();
    }

    // Emit copies from args into vregs at the entry.
    const auto &TRI = *MF->getSubtarget().getRegisterInfo();
    RegInfo->EmitLiveInCopies(entry, TRI, *TII);

    TLI->finalizeLowering(*MF);

    blocks_.clear();
    DAGSize_ = 0;
    MBB_ = nullptr;
    MF = nullptr;
  }

  return true;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerData(const Data *data)
{
  for (const Atom &atom : *data) {
    new llvm::GlobalVariable(
        *M,
        voidTy_,
        false,
        llvm::GlobalValue::ExternalLinkage,
        nullptr,
        atom.GetSymbol()->GetName().data()
    );
  }
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const Inst *i)
{
  if (i->IsTerminator()) {
    HandleSuccessorPHI(i->GetParent());
  }

  switch (i->GetKind()) {
    // Control flow.
    case Inst::Kind::CALL:   LowerCall(static_cast<const CallInst *>(i)); break;
    case Inst::Kind::TCALL:  LowerTailCall(static_cast<const TailCallInst *>(i)); break;
    case Inst::Kind::INVOKE: LowerInvoke(static_cast<const InvokeInst *>(i)); break;
    case Inst::Kind::RET:    LowerReturn(static_cast<const ReturnInst *>(i)); break;
    case Inst::Kind::JCC:    LowerJCC(static_cast<const JumpCondInst *>(i)); break;
    case Inst::Kind::JI:     LowerJI(static_cast<const JumpIndirectInst *>(i)); break;
    case Inst::Kind::JMP:    LowerJMP(static_cast<const JumpInst *>(i)); break;
    case Inst::Kind::SWITCH: LowerSwitch(static_cast<const SwitchInst *>(i)); break;
    case Inst::Kind::TRAP:   LowerTrap(static_cast<const TrapInst *>(i)); break;
    // Memory.
    case Inst::Kind::LD:     LowerLD(static_cast<const LoadInst *>(i)); break;
    case Inst::Kind::ST:     LowerST(static_cast<const StoreInst *>(i)); break;
    case Inst::Kind::PUSH:   LowerPush(static_cast<const PushInst *>(i)); break;
    case Inst::Kind::POP:    LowerPop(static_cast<const PopInst *>(i)); break;
    // Atomic exchange.
    case Inst::Kind::XCHG:   LowerXCHG(static_cast<const ExchangeInst *>(i)); break;
    // Set register.
    case Inst::Kind::SET:    LowerSet(static_cast<const SetInst *>(i)); break;
    // Constant.
    case Inst::Kind::IMM:    LowerImm(static_cast<const ImmInst *>(i)); break;
    case Inst::Kind::ADDR:   LowerAddr(static_cast<const AddrInst *>(i)); break;
    case Inst::Kind::ARG:    LowerArg(static_cast<const ArgInst *>(i));  break;
    // Conditional.
    case Inst::Kind::SELECT: LowerSelect(static_cast<const SelectInst *>(i)); break;
    // Unary instructions.
    case Inst::Kind::ABS:    LowerUnary(i, ISD::FABS);
    case Inst::Kind::NEG:    LowerUnary(i, ISD::FNEG);
    case Inst::Kind::MOV:    LowerMov(static_cast<const MovInst *>(i)); break;
    case Inst::Kind::SEXT:   LowerUnary(i, ISD::SIGN_EXTEND); break;
    case Inst::Kind::ZEXT:   LowerUnary(i, ISD::ZERO_EXTEND); break;
    case Inst::Kind::TRUNC:  LowerUnary(i, ISD::TRUNCATE); break;
    // Binary instructions.
    case Inst::Kind::CMP:    LowerCmp(static_cast<const CmpInst *>(i)); break;
    case Inst::Kind::DIV:    LowerBinary(i, ISD::UDIV, ISD::SDIV, ISD::FDIV); break;
    case Inst::Kind::REM:    LowerBinary(i, ISD::UREM, ISD::SREM, ISD::FREM); break;
    case Inst::Kind::MUL:    LowerBinary(i, ISD::MUL,  ISD::MUL,  ISD::FMUL); break;
    case Inst::Kind::ADD:    LowerBinary(i, ISD::ADD,  ISD::ADD,  ISD::FADD); break;
    case Inst::Kind::SUB:    LowerBinary(i, ISD::SUB,  ISD::SUB,  ISD::FSUB); break;
    case Inst::Kind::AND:    LowerBinary(i, ISD::AND); break;
    case Inst::Kind::OR:     LowerBinary(i, ISD::OR);  break;
    case Inst::Kind::SLL:    LowerBinary(i, ISD::SHL); break;
    case Inst::Kind::SRA:    LowerBinary(i, ISD::SRA); break;
    case Inst::Kind::SRL:    LowerBinary(i, ISD::SRL); break;
    case Inst::Kind::XOR:    LowerBinary(i, ISD::XOR); break;
    case Inst::Kind::ROTL:   LowerBinary(i, ISD::ROTL); break;
    // PHI node.
    case Inst::Kind::PHI:    break;
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerBinary(const Inst *inst, unsigned op)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);

  SDNodeFlags flags;
  MVT type = GetType(binaryInst->GetType());
  SDValue lhs = GetValue(binaryInst->GetLHS());
  SDValue rhs = GetValue(binaryInst->GetRHS());
  SDValue bin = CurDAG->getNode(op, SDL_, type, lhs, rhs, flags);
  values_[inst] = bin;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerBinary(
    const Inst *inst,
    unsigned sop,
    unsigned uop,
    unsigned fop)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);
  switch (binaryInst->GetType()) {
    case Type::I8: case Type::I16: case Type::I32: case Type::I64: {
      LowerBinary(inst, sop);
      break;
    }
    case Type::U8: case Type::U16: case Type::U32: case Type::U64: {
      LowerBinary(inst, uop);
      break;
    }
    case Type::F32: case Type::F64: {
      LowerBinary(inst, fop);
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerUnary(const Inst *inst, unsigned opcode)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJCC(const JumpCondInst *inst)
{
  auto *sourceMBB = blocks_[inst->GetParent()];
  auto *trueMBB = blocks_[inst->GetTrueTarget()];
  auto *falseMBB = blocks_[inst->GetFalseTarget()];

  Chain = CurDAG->getNode(
      ISD::BRCOND,
      SDL_,
      MVT::Other,
      Chain,
      GetValue(inst->GetCond()),
      CurDAG->getBasicBlock(blocks_[inst->GetTrueTarget()])
  );
  Chain = CurDAG->getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      Chain,
      CurDAG->getBasicBlock(blocks_[inst->GetFalseTarget()])
  );

  sourceMBB->addSuccessor(trueMBB);
  sourceMBB->addSuccessor(falseMBB);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJI(const JumpIndirectInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJMP(const JumpInst *inst)
{
  Block *target = inst->getSuccessor(0);
  auto *sourceMBB = blocks_[inst->GetParent()];
  auto *targetMBB = blocks_[target];

  Chain = CurDAG->getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      Chain,
      CurDAG->getBasicBlock(targetMBB)
  );

  sourceMBB->addSuccessor(targetMBB);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSwitch(const SwitchInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerLD(const LoadInst *ld)
{
  bool sign;
  size_t size;
  switch (ld->GetType()) {
    case Type::I8:  sign = true;  size = 1; break;
    case Type::I16: sign = true;  size = 2; break;
    case Type::I32: sign = true;  size = 4; break;
    case Type::I64: sign = true;  size = 8; break;
    case Type::U8:  sign = false; size = 1; break;
    case Type::U16: sign = false; size = 2; break;
    case Type::U32: sign = false; size = 4; break;
    case Type::U64: sign = false; size = 8; break;
    case Type::F32: assert(!"not implemented");
    case Type::F64: assert(!"not implemented");
  }

  ISD::LoadExtType ext;
  if (size > ld->GetLoadSize()) {
    ext = sign ? ISD::SEXTLOAD : ISD::ZEXTLOAD;
  } else {
    ext = ISD::NON_EXTLOAD;
  }

  MVT mt;
  switch (ld->GetLoadSize()) {
    case 1: mt = MVT::i8;  break;
    case 2: mt = MVT::i16; break;
    case 4: mt = MVT::i32; break;
    case 8: mt = MVT::i64; break;
    default: assert(!"not implemented");
  }

  SDValue l = CurDAG->getExtLoad(
      ext,
      SDL_,
      GetType(ld->GetType()),
      Chain,
      GetValue(ld->GetAddr()),
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      mt
  );

  Chain = l.getValue(1);
  values_[ld] = Chain;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerST(const StoreInst *st)
{
  MVT mt;
  switch (st->GetStoreSize()) {
    case 1: mt = MVT::i8;  break;
    case 2: mt = MVT::i16; break;
    case 4: mt = MVT::i32; break;
    case 8: mt = MVT::i64; break;
    default: assert(!"not implemented");
  }

  Chain = CurDAG->getTruncStore(
      Chain,
      SDL_,
      GetValue(st->GetVal()),
      GetValue(st->GetAddr()),
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      mt
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerReturn(const ReturnInst *retInst)
{
  llvm::SmallVector<SDValue, 6> returns;
  returns.push_back(SDValue());
  returns.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));

  SDValue flag;
  if (auto *retVal = retInst->GetValue()) {
    Type retType = retVal->GetType(0);
    unsigned retReg;
    switch (retType) {
      case Type::I64: retReg = X86::RAX; break;
      case Type::I32: retReg = X86::EAX; break;
      default: assert(!"not implemented");
    }

    SDValue arg = GetValue(retVal);
    Chain = CurDAG->getCopyToReg(Chain, SDL_, retReg, arg, flag);
    returns.push_back(CurDAG->getRegister(retReg, GetType(retType)));
    flag = Chain.getValue(1);
  }

  returns[0] = Chain;
  if (flag.getNode()) {
    returns.push_back(flag);
  }

  Chain = CurDAG->getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, returns);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCall(const CallInst *inst)
{
  assert(!"not implemented");
}


// -----------------------------------------------------------------------------
void X86ISel::LowerTailCall(const TailCallInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerInvoke(const InvokeInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerImm(const ImmInst *imm)
{
  auto i = [this, imm](auto t) {
    values_[imm] = CurDAG->getConstant(imm->GetInt(), SDL_, t);
  };
  auto f = [this, imm](auto t) {
    values_[imm] = CurDAG->getConstantFP(imm->GetFloat(), SDL_, t);
  };

  switch (imm->GetType()) {
    case Type::I8:  i(MVT::i8);  break;
    case Type::I16: i(MVT::i16); break;
    case Type::I32: i(MVT::i32); break;
    case Type::I64: i(MVT::i64); break;
    case Type::U8:  i(MVT::i8);  break;
    case Type::U16: i(MVT::i16); break;
    case Type::U32: i(MVT::i32); break;
    case Type::U64: i(MVT::i64); break;
    case Type::F32: f(MVT::f32); break;
    case Type::F64: f(MVT::f64); break;
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerAddr(const AddrInst *addr)
{
  const auto &op = addr->GetOp(0);
  if (op.IsSym()) {
    const std::string_view name = op.GetSym()->GetName();
    if (auto *GV = M->getNamedValue(name.data())) {
      values_[addr] = CurDAG->getGlobalAddress(GV, SDL_, MVT::i64);
    } else {
      throw std::runtime_error("Unknown symbol: " + std::string(name));
    }
  } else {
    assert(!"not implemented");
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArg(const ArgInst *argInst)
{
  const llvm::TargetRegisterClass *RC;
  MVT RegVT;
  unsigned ArgReg;

  switch (argInst->GetType()) {
    case Type::U8:  case Type::I8:  assert(!"not implemented");
    case Type::U16: case Type::I16: assert(!"not implemented");
    case Type::U32: case Type::I32: {
      RegVT = MVT::i32;
      RC = &X86::GR32RegClass;
      switch (argInst->GetIdx()) {
        case 0: ArgReg = X86::EDI; break;
        case 1: ArgReg = X86::ESI; break;
        case 2: ArgReg = X86::ECX; break;
        case 3: ArgReg = X86::EDX; break;
        case 4: ArgReg = X86::R8D; break;
        case 5: ArgReg = X86::R9D; break;
        default: assert(!"not implemented");
      }
      break;
    }
    case Type::U64: case Type::I64: {
      RegVT = MVT::i64;
      RC = &X86::GR64RegClass;
      switch (argInst->GetIdx()) {
        case 0: ArgReg = X86::RDI; break;
        case 1: ArgReg = X86::RSI; break;
        case 2: ArgReg = X86::RCX; break;
        case 3: ArgReg = X86::RDX; break;
        case 4: ArgReg = X86::R8;  break;
        case 5: ArgReg = X86::R9;  break;
        default: assert(!"not implemented");
      }
      break;
    }
    case Type::F32: assert(!"not implemented");
    case Type::F64: assert(!"not implemented");
  }

  unsigned Reg = MF->addLiveIn(ArgReg, RC);
  SDValue arg = CurDAG->getCopyFromReg(Chain, SDL_, Reg, RegVT);
  values_[argInst] = arg;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCmp(const CmpInst *cmpInst)
{
  SDNodeFlags flags;
  MVT type = GetType(cmpInst->GetType());
  SDValue lhs = GetValue(cmpInst->GetLHS());
  SDValue rhs = GetValue(cmpInst->GetRHS());
  ISD::CondCode cc = GetCond(cmpInst->GetCC());
  SDValue cmp = CurDAG->getSetCC(SDL_, MVT::i32, lhs, rhs, cc);
  values_[cmpInst] = cmp;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTrap(const TrapInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerMov(const MovInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerPush(const PushInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerPop(const PopInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerXCHG(const ExchangeInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSet(const SetInst *inst)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSelect(const SelectInst *select)
{
  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
void X86ISel::HandleSuccessorPHI(const Block *block)
{
  llvm::SmallPtrSet<llvm::MachineBasicBlock *, 4> handled;
  auto *RegInfo = &MF->getRegInfo();
  auto *blockMBB = blocks_[block];

  for (const Block *succBB : block->successors()) {
    llvm::MachineBasicBlock *succMBB = blocks_[succBB];
    if (!handled.insert(succMBB).second) {
      continue;
    }

    auto phiIt = succMBB->begin();
    for (const PhiInst &phi : succBB->phis()) {
      llvm::MachineInstrBuilder mPhi(*MF, phiIt++);
      const auto &val = phi.GetValue(block);
      unsigned reg = 0;
      MVT VT = GetType(phi.GetType());

      switch (val.GetKind()) {
        case Operand::Kind::INT: {
          reg = RegInfo->createVirtualRegister(TLI->getRegClassFor(VT));
          SDValue intVal = CurDAG->getConstant(val.GetInt(), SDL_, VT);
          Chain = CurDAG->getCopyToReg(Chain, SDL_, reg, intVal);
          break;
        }
        case Operand::Kind::FLOAT: assert(!"not implemented");
        case Operand::Kind::REG:   assert(!"not implemented");
        case Operand::Kind::INST: {
          auto it = regs_.find(val.GetInst());
          if (it != regs_.end()) {
            reg = it->second;
          } else {
            assert(!"not implemented");
          }
          break;
        }
        case Operand::Kind::SYM:   assert(!"not implemented");
        case Operand::Kind::EXPR:  assert(!"not implemented");
        case Operand::Kind::BLOCK: assert(!"not implemented");
        case Operand::Kind::UNDEF: {
          assert(!"not implemented");
          break;
        }
      }

      mPhi.addReg(reg).addMBB(blockMBB);
    }
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::GetValue(const Inst *inst)
{
  auto vt = values_.find(inst);
  if (vt != values_.end()) {
    return vt->second;
  }

  auto rt = regs_.find(inst);
  if (rt != regs_.end()) {
    return CurDAG->getCopyFromReg(
        CurDAG->getEntryNode(),
        SDL_,
        rt->second,
        GetType(inst->GetType(0))
    );
  }

  assert(!"not implemented");
}

// -----------------------------------------------------------------------------
llvm::MVT X86ISel::GetType(Type t)
{
  switch (t) {
    case Type::I8:  return MVT::i8;
    case Type::I16: return MVT::i16;
    case Type::I32: return MVT::i32;
    case Type::I64: return MVT::i64;
    case Type::U8:  return MVT::i8;
    case Type::U16: return MVT::i16;
    case Type::U32: return MVT::i32;
    case Type::U64: return MVT::i64;
    case Type::F32: return MVT::f32;
    case Type::F64: return MVT::f64;
  }
}

// -----------------------------------------------------------------------------
ISD::CondCode X86ISel::GetCond(Cond cc)
{
  switch (cc) {
    case Cond::EQ:  return ISD::CondCode::SETEQ;
    case Cond::NEQ: return ISD::CondCode::SETNE;
    case Cond::LE:  return ISD::CondCode::SETLE;
    case Cond::LT:  return ISD::CondCode::SETLT;
    case Cond::GE:  return ISD::CondCode::SETGE;
    case Cond::GT:  return ISD::CondCode::SETGT;
    case Cond::OLE: return ISD::CondCode::SETOLE;
    case Cond::OLT: return ISD::CondCode::SETOLT;
    case Cond::OGE: return ISD::CondCode::SETOGE;
    case Cond::OGT: return ISD::CondCode::SETOGT;
    case Cond::ULE: return ISD::CondCode::SETULE;
    case Cond::ULT: return ISD::CondCode::SETULT;
    case Cond::UGE: return ISD::CondCode::SETUGE;
    case Cond::UGT: return ISD::CondCode::SETUGT;
  }
}

// -----------------------------------------------------------------------------
void X86ISel::CodeGenAndEmitDAG()
{
  bool Changed;

  llvm::AliasAnalysis *AA = nullptr;

  CurDAG->NewNodesMustHaveLegalTypes = false;
  CurDAG->Combine(llvm::BeforeLegalizeTypes, AA, OptLevel);
  Changed = CurDAG->LegalizeTypes();
  CurDAG->NewNodesMustHaveLegalTypes = true;

  if (Changed) {
    CurDAG->Combine(llvm::AfterLegalizeTypes, AA, OptLevel);
  }

  Changed = CurDAG->LegalizeVectors();

  if (Changed) {
    CurDAG->LegalizeTypes();
    CurDAG->Combine(llvm::AfterLegalizeVectorOps, AA, OptLevel);
  }

  CurDAG->Legalize();
  CurDAG->Combine(llvm::AfterLegalizeDAG, AA, OptLevel);

  DoInstructionSelection();

  llvm::ScheduleDAGSDNodes *Scheduler = CreateScheduler();
  Scheduler->Run(CurDAG, MBB_);

  llvm::MachineBasicBlock *Fst = MBB_;
  MBB_ = Scheduler->EmitSchedule(insert_);
  llvm::MachineBasicBlock *Snd = MBB_;

  if (Fst != Snd) {
    assert(!"not implemented");
  }
  delete Scheduler;

  CurDAG->clear();
}

// -----------------------------------------------------------------------------
class ISelUpdater : public SelectionDAG::DAGUpdateListener {
  SelectionDAG::allnodes_iterator &ISelPosition;

public:
  ISelUpdater(SelectionDAG &DAG, SelectionDAG::allnodes_iterator &isp)
    : SelectionDAG::DAGUpdateListener(DAG), ISelPosition(isp)
  {
  }

  void NodeDeleted(SDNode *N, SDNode *E) override {
    if (ISelPosition == SelectionDAG::allnodes_iterator(N)) {
      ++ISelPosition;
    }
  }
};

// -----------------------------------------------------------------------------
void X86ISel::DoInstructionSelection()
{
  DAGSize_ = CurDAG->AssignTopologicalOrder();

  llvm::HandleSDNode Dummy(CurDAG->getRoot());
  SelectionDAG::allnodes_iterator ISelPosition(CurDAG->getRoot().getNode());
  ++ISelPosition;

  ISelUpdater ISU(*CurDAG, ISelPosition);

  while (ISelPosition != CurDAG->allnodes_begin()) {
    SDNode *Node = &*--ISelPosition;
    if (Node->use_empty()) {
      continue;
    }
    if (Node->isStrictFPOpcode()) {
      Node = CurDAG->mutateStrictFPToFP(Node);
    }

    Select(Node);
  }

  CurDAG->setRoot(Dummy.getValue());
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *X86ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}

// -----------------------------------------------------------------------------
llvm::StringRef X86ISel::getPassName() const
{
  return "GenM -> DAG pass";
}

// -----------------------------------------------------------------------------
void X86ISel::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfo>();
  AU.addPreserved<llvm::MachineModuleInfo>();
}
