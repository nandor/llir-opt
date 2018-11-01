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
    TargetLibraryInfo *TLI,
    const Prog *prog)
  : ModulePass(ID)
  , TM_(TM)
  , STI_(STI)
  , TII_(TII)
  , TLI_(TLI)
  , prog_(prog)
  , DAG_(*TM, CodeGenOpt::Aggressive)
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
    auto &MF = MMI.getOrCreateMachineFunction(*F);

    // Create an entry block, lowering all arguments.
    DAG_.init(MF, *ORE, this, TLI_, nullptr);

    // Create the entry machine basic block.
    MBB = MF.CreateMachineBasicBlock(nullptr);
    MF.push_back(MBB);
    BuildMI(MBB, DL_, TII_->get(X86::RETQ));
  }

  return true;
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
