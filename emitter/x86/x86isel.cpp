// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
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
    const Prog *prog)
  : ModulePass(ID)
  , TM_(TM)
  , STI_(STI)
  , TII_(TII)
  , TRI_(TRI)
  , TLI_(TLI)
  , LibInfo_(LibInfo)
  , prog_(prog)
  , opt_(CodeGenOpt::Aggressive)
  , DAG_(*TM, opt_)
  , MF_(nullptr)
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
    MF_ = &MMI.getOrCreateMachineFunction(*F);

    // Create an entry block, lowering all arguments.
    DAG_.init(*MF_, *ORE, this, LibInfo_, nullptr);

    // Create the entry machine basic block.
    MBB_ = MF_->CreateMachineBasicBlock(nullptr);
    MF_->push_back(MBB_);

    insert_ = MBB_->end();

    SDValue Chain = DAG_.getRoot();
    SmallVector<SDValue, 6> RetOps;
    RetOps.push_back(Chain);
    RetOps.push_back(DAG_.getTargetConstant(0, SDL_, MVT::i32));
    DAG_.getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, RetOps);

    CodeGenAndEmitDAG();
  }

  return true;
}

// -----------------------------------------------------------------------------
void X86ISel::CodeGenAndEmitDAG()
{
  bool Changed;

  AliasAnalysis *AA = nullptr;

  DAG_.NewNodesMustHaveLegalTypes = false;
  DAG_.Combine(BeforeLegalizeTypes, AA, opt_);
  Changed = DAG_.LegalizeTypes();
  DAG_.NewNodesMustHaveLegalTypes = true;

  if (Changed) {
    DAG_.Combine(AfterLegalizeTypes, AA, opt_);
  }

  Changed = DAG_.LegalizeVectors();

  if (Changed) {
    DAG_.LegalizeTypes();
    DAG_.Combine(AfterLegalizeVectorOps, AA, opt_);
  }

  DAG_.Legalize();
  DAG_.Combine(AfterLegalizeDAG, AA, opt_);

  DoInstructionSelection();

  ScheduleDAGSDNodes *Scheduler = CreateScheduler();
  Scheduler->Run(&DAG_, MBB_);
  MBB_ = Scheduler->EmitSchedule(insert_);

  delete Scheduler;

  DAG_.clear();
}

// -----------------------------------------------------------------------------
void X86ISel::DoInstructionSelection()
{
  DAGSize_ = DAG_.AssignTopologicalOrder();

  HandleSDNode Dummy(DAG_.getRoot());
  SelectionDAG::allnodes_iterator ISelPosition(DAG_.getRoot().getNode());
  ++ISelPosition;

  // ISelUpdater ISU(*DAG_, ISelPosition);

  while (ISelPosition != DAG_.allnodes_begin()) {
    SDNode *Node = &*--ISelPosition;
    if (Node->use_empty()) {
      continue;
    }
    if (Node->isStrictFPOpcode()) {
      Node = DAG_.mutateStrictFPToFP(Node);
    }

    assert(!"not implemented");
    //Select(Node);
  }

  DAG_.setRoot(Dummy.getValue());
}

// -----------------------------------------------------------------------------
ScheduleDAGSDNodes *X86ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF_, TII_, TRI_, TLI_, opt_);
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
