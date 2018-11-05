// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>
#include "core/block.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/prog.h"
#include "emitter/x86/x86isel.h"

using namespace llvm;

#include <iostream>

// -----------------------------------------------------------------------------
char X86ISel::ID;

// -----------------------------------------------------------------------------
X86ISel::X86ISel(
    X86TargetMachine *TM,
    X86Subtarget *STI,
    X86InstrInfo *TII,
    X86RegisterInfo *TRI,
    TargetLowering *TLI,
    TargetLibraryInfo *LibInfo,
    const Prog *prog,
    llvm::CodeGenOpt::Level OL)
  : X86DAGMatcher(*TM, OL, STI)
  , DAGMatcher(*TM, new SelectionDAG(*TM, OL), OL, TLI, TII)
  , ModulePass(ID)
  , TRI_(TRI)
  , LibInfo_(LibInfo)
  , prog_(prog)
  , MBB_(nullptr)
{
}

// -----------------------------------------------------------------------------
bool X86ISel::runOnModule(Module &M)
{
  funcTy_ = FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {});

  auto &MMI = getAnalysis<MachineModuleInfo>();
  for (const Func &func : *prog_) {
    // Empty function - skip it.
    if (func.IsEmpty()) {
      continue;
    }

    // Create a new dummy empty Function.
    // The IR function simply returns void since it cannot be empty.
    auto *C = M.getOrInsertFunction(std::string(func.GetName()), funcTy_);
    auto *F = dyn_cast<Function>(C);
    BasicBlock* block = BasicBlock::Create(F->getContext(), "entry", F);
    IRBuilder<> builder(block);
    builder.CreateRetVoid();

    // Create a MachineFunction, attached to the dummy one.
    auto ORE = make_unique<OptimizationRemarkEmitter>(F);
    MF = &MMI.getOrCreateMachineFunction(*F);

    // Initialise the dag with info for this function.
    CurDAG->init(*MF, *ORE, this, LibInfo_, nullptr);

    // Create a MBB for all GenM blocks, isolating the entry block.
    DenseMap<const Block *, MachineBasicBlock *> blockMap;
    MachineBasicBlock *entry = nullptr;
    for (const auto &block : func) {
      MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(nullptr);
      blockMap[&block] = MBB;
      entry = entry ? entry : MBB;
    }

    // Lower individual blocks.
    for (const auto &block : func) {
      MBB_ = blockMap[&block];

      // Set up the SelectionDAG for the block.
      for (const auto &inst : block) {
        Lower(&inst);
      }

      CurDAG->dump();
      /*
      SDValue Chain = CurDAG->getRoot();
      SmallVector<SDValue, 6> RetOps;
      RetOps.push_back(Chain);
      RetOps.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));
      SDValue ret = CurDAG->getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, RetOps);
      CurDAG->setRoot(ret);
      */

      // Lower the block.
      insert_ = MBB_->end();
      CodeGenAndEmitDAG();
      MF->push_back(MBB_);
    }

    TLI->finalizeLowering(*MF);
  }

  return true;
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const Inst *inst)
{
  /*
  switch (inst->GetKind()) {
    // Control flow.
    case Inst::Kind::CALL: {
      llvm::errs() << "CALL\n";
      break;
    }
    case Inst::Kind::TCALL: {
      assert(!"not implemented");
    }
    case Inst::Kind::JT: {
      llvm::errs() << "JT\n";
      break;
    }
    case Inst::Kind::JF: {
      assert(!"not implemented");
    }
    case Inst::Kind::JI: {
      assert(!"not implemented");
    }
    case Inst::Kind::JMP: {
      assert(!"not implemented");
    }
    case Inst::Kind::RET: {
      llvm::errs() << "RET\n";
      break;
    }
    case Inst::Kind::SWITCH: {
      assert(!"not implemented");
    }
    // Memory.
    case Inst::Kind::LD: {
      llvm::errs() << "LD\n";
      break;
    }
    case Inst::Kind::ST: {
      llvm::errs() << "ST\n";
      break;
    }
    case Inst::Kind::PUSH: {
      assert(!"not implemented");
    }
    case Inst::Kind::POP: {
      assert(!"not implemented");
    }
    // Atomic exchange.
    case Inst::Kind::XCHG: {
      assert(!"not implemented");
    }
    // Set register.
    case Inst::Kind::SET: {
      assert(!"not implemented");
    }
    // Constant.
    case Inst::Kind::IMM: {
      llvm::errs() << "IMM\n";
      break;
    }
    case Inst::Kind::ADDR: {
      llvm::errs() << "ADDR\n";
      break;
    }
    case Inst::Kind::ARG: {
      llvm::errs() << "ARG\n";
      break;
    }
    // Conditional.
    case Inst::Kind::SELECT: {
      assert(!"not implemented");
    }
    // Unary instructions.
    case Inst::Kind::ABS: {
      assert(!"not implemented");
    }
    case Inst::Kind::MOV: {
      assert(!"not implemented");
    }
    case Inst::Kind::NEG: {
      assert(!"not implemented");
    }
    case Inst::Kind::SEXT: {
      assert(!"not implemented");
    }
    case Inst::Kind::ZEXT: {
      assert(!"not implemented");
    }
    case Inst::Kind::TRUNC: {
      assert(!"not implemented");
    }
    // Binary instructions.
    case Inst::Kind::ADD: LowerBinary(inst, ISD::ADD); break;
    case Inst::Kind::AND: LowerBinary(inst, ISD::AND); break;
    case Inst::Kind::ASR: LowerBinary(inst, ISD::SRA); break;
    case Inst::Kind::CMP: {
      llvm::errs() << "CMP\n";
      break;
    }
    case Inst::Kind::DIV: {
      assert(!"not implemented");
      break;
    }
    case Inst::Kind::LSL: LowerBinary(inst, ISD::SHL); break;
    case Inst::Kind::LSR: LowerBinary(inst, ISD::SRL); break;
    case Inst::Kind::MOD: {
      assert(!"not implemented");
    }
    case Inst::Kind::MUL: {
      assert(!"not implemented");
    }
    case Inst::Kind::MULH: {
      assert(!"not implemented");
    }
    case Inst::Kind::OR: LowerBinary(inst, ISD::OR); break;
    case Inst::Kind::ROTL: {
      assert(!"not implemented");
    }
    case Inst::Kind::SHL: {
      assert(!"not implemented");
    }
    case Inst::Kind::REM: {
      assert(!"not implemented");
    }
    case Inst::Kind::SRL: {
      assert(!"not implemented");
    }
    case Inst::Kind::SUB: {
      assert(!"not implemented");
    }
    case Inst::Kind::XOR: {
      assert(!"not implemented");
    }
    // PHI node.
    case Inst::Kind::PHI: {
      assert(!"not implemented");
    }
  }
  */
}

// -----------------------------------------------------------------------------
void X86ISel::LowerUnary(const Inst *inst, unsigned opcode)
{
  llvm::errs() << "Unary";
}

// -----------------------------------------------------------------------------
void X86ISel::LowerBinary(const Inst *inst, unsigned opcode)
{
  llvm::errs() << "Binary";
}

// -----------------------------------------------------------------------------
void X86ISel::CodeGenAndEmitDAG()
{
  bool Changed;

  AliasAnalysis *AA = nullptr;

  CurDAG->NewNodesMustHaveLegalTypes = false;
  CurDAG->Combine(BeforeLegalizeTypes, AA, OptLevel);
  Changed = CurDAG->LegalizeTypes();
  CurDAG->NewNodesMustHaveLegalTypes = true;

  if (Changed) {
    CurDAG->Combine(AfterLegalizeTypes, AA, OptLevel);
  }

  Changed = CurDAG->LegalizeVectors();

  if (Changed) {
    CurDAG->LegalizeTypes();
    CurDAG->Combine(AfterLegalizeVectorOps, AA, OptLevel);
  }

  CurDAG->Legalize();
  CurDAG->Combine(AfterLegalizeDAG, AA, OptLevel);

  DoInstructionSelection();

  ScheduleDAGSDNodes *Scheduler = CreateScheduler();
  Scheduler->Run(CurDAG, MBB_);

  MachineBasicBlock *Fst = MBB_;
  MBB_ = Scheduler->EmitSchedule(insert_);
  MachineBasicBlock *Snd = MBB_;

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

  HandleSDNode Dummy(CurDAG->getRoot());
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
ScheduleDAGSDNodes *X86ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}

// -----------------------------------------------------------------------------
StringRef X86ISel::getPassName() const
{
  return "GenM -> DAG pass";
}

// -----------------------------------------------------------------------------
void X86ISel::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<MachineModuleInfo>();
  AU.addPreserved<MachineModuleInfo>();
}
