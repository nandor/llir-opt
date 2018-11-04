// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>
#include "core/prog.h"
#include "core/func.h"
#include "emitter/x86/x86isel.h"

using namespace llvm;



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
  funcTy_ = FunctionType::get(Type::getVoidTy(M.getContext()), {});

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

    // Create an entry block, lowering all arguments.
    CurDAG->init(*MF, *ORE, this, LibInfo_, nullptr);

    // Create the entry machine basic block.
    MBB_ = MF->CreateMachineBasicBlock(nullptr);
    MF->push_back(MBB_);

    insert_ = MBB_->end();

    SDValue Chain = CurDAG->getRoot();

    SmallVector<SDValue, 6> RetOps;
    RetOps.push_back(Chain);
    RetOps.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));
    SDValue ret = CurDAG->getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, RetOps);

    CurDAG->setRoot(ret);

    CodeGenAndEmitDAG();

    TLI->finalizeLowering(*MF);
  }

  return true;
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
