// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>
#include "core/block.h"
#include "core/context.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/prog.h"
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
bool X86ISel::runOnModule(llvm::Module &Module)
{
  M = &Module;

  auto &Ctx = M->getContext();
  voidTy_ = llvm::Type::getVoidTy(Ctx);
  funcTy_ = llvm::FunctionType::get(voidTy_, {});

  // Populate the symbol table.
  for (const Func &func : *prog_) {
    for (const auto &block : func) {
      for (const auto &inst : block) {
        if (inst.GetKind() != Inst::Kind::ADDR) {
          continue;
        }
        auto *addrInst = static_cast<const AddrInst*>(&inst);
        new llvm::GlobalVariable(
            *M,
            voidTy_,
            false,
            llvm::GlobalValue::ExternalLinkage,
            nullptr,
            addrInst->GetSymbolName()
        );
      }
    }
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

    // Create a MBB for all GenM blocks, isolating the entry block.
    llvm::MachineBasicBlock *entry = nullptr;
    for (const auto &block : func) {
      llvm::MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(nullptr);
      blocks_[&block] = MBB;
      entry = entry ? entry : MBB;
    }

    // Lower individual blocks.
    for (const auto &block : func) {
      llvm::errs() << block.GetName().data() << "\n";
      MBB_ = blocks_[&block];
      MF->push_back(MBB_);

      // Set up the SelectionDAG for the block.
      Chain = CurDAG->getRoot();
      for (const auto &inst : block) {
        Lower(&inst);
      }
      CurDAG->setRoot(Chain);
      CurDAG->dump();

      // Lower the block.
      insert_ = MBB_->end();
      CodeGenAndEmitDAG();
    }

    // Emit copies from args into vregs at the entry.
    const auto &TRI = *MF->getSubtarget().getRegisterInfo();
    auto *RegInfo = &MF->getRegInfo();
    RegInfo->EmitLiveInCopies(entry, TRI, *TII);

    entry->dump();

    TLI->finalizeLowering(*MF);

    blocks_.clear();
    values_.clear();
    DAGSize_ = 0;
    MBB_ = nullptr;
    MF = nullptr;
  }

  return true;
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const Inst *i)
{
  switch (i->GetKind()) {
    // Control flow.
    case Inst::Kind::CALL:   LowerCall(i); break;
    case Inst::Kind::TCALL:  assert(!"not implemented");
    case Inst::Kind::JT:     LowerJT(static_cast<const JumpTrueInst *>(i)); break;
    case Inst::Kind::JF:     LowerJF(static_cast<const JumpFalseInst *>(i)); break;
    case Inst::Kind::JI:     assert(!"not implemented");
    case Inst::Kind::JMP:    assert(!"not implemented");
    case Inst::Kind::RET:    LowerReturn(static_cast<const ReturnInst *>(i)); break;
    case Inst::Kind::SWITCH: assert(!"not implemented");
    // Memory.
    case Inst::Kind::LD:     LowerLD(static_cast<const LoadInst *>(i)); break;
    case Inst::Kind::ST:     LowerST(static_cast<const StoreInst *>(i)); break;
    case Inst::Kind::PUSH:   assert(!"not implemented");
    case Inst::Kind::POP:    assert(!"not implemented");
    // Atomic exchange.
    case Inst::Kind::XCHG:   assert(!"not implemented");
    // Set register.
    case Inst::Kind::SET:    assert(!"not implemented");
    // Constant.
    case Inst::Kind::IMM:    LowerImm(static_cast<const ImmInst *>(i)); break;
    case Inst::Kind::ADDR:   LowerAddr(static_cast<const AddrInst *>(i)); break;
    case Inst::Kind::ARG:    LowerArg(static_cast<const ArgInst *>(i));  break;
    // Conditional.
    case Inst::Kind::SELECT: assert(!"not implemented");
    // Unary instructions.
    case Inst::Kind::ABS:    assert(!"not implemented");
    case Inst::Kind::MOV:    assert(!"not implemented");
    case Inst::Kind::NEG:    assert(!"not implemented");
    case Inst::Kind::SEXT:   assert(!"not implemented");
    case Inst::Kind::ZEXT:   assert(!"not implemented");
    case Inst::Kind::TRUNC:  assert(!"not implemented");
    // Binary instructions.
    case Inst::Kind::CMP:    LowerCmp(static_cast<const CmpInst *>(i)); break;
    case Inst::Kind::DIV:    assert(!"not implemented");
    case Inst::Kind::MOD:    assert(!"not implemented");
    case Inst::Kind::MUL:    assert(!"not implemented");
    case Inst::Kind::MULH:   assert(!"not implemented");
    case Inst::Kind::ROTL:   assert(!"not implemented");
    case Inst::Kind::REM:    assert(!"not implemented");
    case Inst::Kind::ADD:    LowerBinary(i, ISD::ADD); break;
    case Inst::Kind::AND:    LowerBinary(i, ISD::AND); break;
    case Inst::Kind::OR:     LowerBinary(i, ISD::OR);  break;
    case Inst::Kind::SLL:    LowerBinary(i, ISD::SHL); break;
    case Inst::Kind::SRA:    LowerBinary(i, ISD::SRA); break;
    case Inst::Kind::SRL:    LowerBinary(i, ISD::SRL); break;
    case Inst::Kind::SUB:    LowerBinary(i, ISD::SUB); break;
    case Inst::Kind::XOR:    LowerBinary(i, ISD::XOR); break;
    // PHI node.
    case Inst::Kind::PHI:    assert(!"not implemented");
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerBinary(const Inst *inst, unsigned opcode)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);

  SDNodeFlags flags;
  MVT type = GetType(binaryInst->GetType());
  SDValue lhs = GetValue(binaryInst->GetLHS());
  SDValue rhs = GetValue(binaryInst->GetRHS());
  SDValue bin = CurDAG->getNode(opcode, SDL_, type, lhs, rhs, flags);
  values_[inst] = bin;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJT(const JumpTrueInst *inst)
{
  Chain = CurDAG->getNode(
      ISD::BRCOND,
      SDL_,
      MVT::Other,
      Chain,
      GetValue(inst->GetCond()),
      CurDAG->getBasicBlock(blocks_[inst->GetTarget()])
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJF(const JumpFalseInst *inst)
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
void X86ISel::LowerCall(const Inst *inst)
{
  llvm::errs() << "Call\n";
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
  auto *GV = M->getGlobalVariable(addr->GetSymbolName());
  values_[addr] = CurDAG->getGlobalAddress(GV, SDL_, MVT::i64);
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
    case Type::U32: case Type::I32: assert(!"not implemented");

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
llvm::SDValue X86ISel::GetValue(const Inst *inst)
{
  auto it = values_.find(inst);
  if (it == values_.end()) {
    assert(!"not implemented");
  }
  return it->second;
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
