// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <queue>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/CodeGen/MachineFrameInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/CodeGen/TargetFrameLowering.h>
#include <llvm/CodeGen/TargetInstrInfo.h>
#include <llvm/CodeGen/FunctionLoweringInfo.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/LegacyPassManagers.h>

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
using BranchProbability = llvm::BranchProbability;
using GlobalValue = llvm::GlobalValue;


// -----------------------------------------------------------------------------
static bool CompatibleType(Type at, Type it)
{
  if (it == at) {
    return true;
  }
  if (it == Type::V64 || at == Type::V64) {
    return at == Type::I64 || it == Type::I64;
  }
  return false;
}

// -----------------------------------------------------------------------------
static bool UsedOutside(ConstRef<Inst> inst, const Block *block)
{
  std::queue<ConstRef<Inst>> q;
  q.push(inst);

  while (!q.empty()) {
    ConstRef<Inst> i = q.front();
    q.pop();
    for (const Use &use : i->uses()) {
      // The use must be for the specific index.
      if (use != i) {
        continue;
      }

      auto *userInst = cast<const Inst>(use.getUser());
      if (userInst->Is(Inst::Kind::PHI)) {
        return true;
      }
      if (auto *movInst = ::cast_or_null<const MovInst>(userInst)) {
        if (CompatibleType(movInst->GetType(), i->GetType(0))) {
          q.push(movInst);
          continue;
        }
      }
      if (userInst->getParent() != block) {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
ISel::ISel(
    char &ID,
    const Prog &prog,
    llvm::TargetLibraryInfo &libInfo,
    llvm::CodeGenOpt::Level ol)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , libInfo_(libInfo)
  , ol_(ol)
  , MBB_(nullptr)
{
}

// -----------------------------------------------------------------------------
llvm::StringRef ISel::getPassName() const
{
  return "LLIR to LLVM SelectionDAG";
}

// -----------------------------------------------------------------------------
void ISel::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
  AU.addPreserved<llvm::MachineModuleInfoWrapperPass>();
}

// -----------------------------------------------------------------------------
bool ISel::runOnModule(llvm::Module &Module)
{
  M_ = &Module;
  PrepareGlobals();

  // Generate code for functions.
  for (const Func &func : prog_) {
    // Save a pointer to the current function.
    func_ = &func;
    lva_ = nullptr;
    frameIndex_ = 0;
    stackIndices_.clear();

    // Create a new dummy empty Function.
    // The IR function simply returns void since it cannot be empty.
    // Register a handler for more verbose debug info.
    F_ = M_->getFunction(func.getName());
    llvm::PassManagerPrettyStackEntry E(this, *F_);

    // Create a MachineFunction, attached to the dummy one.
    auto *MF = funcs_[&func];
    auto ORE = std::make_unique<llvm::OptimizationRemarkEmitter>(F_);
    if (auto align = func.GetAlignment()) {
      MF->setAlignment(*align);
    }
    Lower(*MF);

    // Get a reference to the underlying DAG.
    auto &DAG = GetDAG();
    auto &MRI = MF->getRegInfo();
    auto &MFI = MF->getFrameInfo();
    const auto &STI = MF->getSubtarget();
    const auto &TRI = *STI.getRegisterInfo();
    const auto &TLI = *STI.getTargetLowering();
    const auto &TII = *STI.getInstrInfo();

    // Initialise the DAG with info for this function.
    llvm::FunctionLoweringInfo FLI;
    DAG.init(*MF, *ORE, this, &libInfo_, nullptr, nullptr, nullptr);
    DAG.setFunctionLoweringInfo(&FLI);

    // Traverse nodes, entry first.
    llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);

    // Flag indicating if the function has VASTART.
    bool hasVAStart = false;

    // Prepare PHIs and arguments.
    for (const Block *block : blockOrder) {
      // First block in reverse post-order is the entry block.
      llvm::MachineBasicBlock *MBB = FLI.MBB = mbbs_[block];

      // Allocate registers for exported values and create PHI
      // instructions for all PHI nodes in the basic block.
      for (const auto &inst : *block) {
        switch (inst.GetKind()) {
          case Inst::Kind::PHI: {
            if (inst.use_empty()) {
              continue;
            }
            // Create a machine PHI instruction for all PHIs. The order of
            // machine PHIs should match the order of PHIs in the block.
            auto &phi = static_cast<const PhiInst &>(inst);
            auto regs = AssignVReg(&phi);
            for (auto &[r, ty] : regs) {
              BuildMI(MBB, DL_, TII.get(llvm::TargetOpcode::PHI), r);
            }
            continue;
          }
          case Inst::Kind::ARG: {
            // If the arg is used outside of entry, export it.
            if (UsedOutside(&inst, &func.getEntryBlock())) {
              AssignVReg(&inst);
            }
            continue;
          }
          case Inst::Kind::VASTART: {
            hasVAStart = true;
            continue;
          }
          case Inst::Kind::TCALL: {
            if (func.IsVarArg()) {
              MFI.setHasMustTailInVarArgFunc(true);
            }
            continue;
          }
          default: {
            // If the value is used outside of the defining block, export it.
            for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
              ConstRef<Inst> ref(&inst, i);
              if (IsExported(ref)) {
                AssignVReg(ref);
              }
            }
          }
        }
      }
    }

    // Lower individual blocks.
    for (const Block *block : blockOrder) {
      llvm::PassManagerPrettyStackEntry E(this, *bbs_[block]);

      MBB_ = mbbs_[block];
      {
        // If this is the entry block, lower all arguments.
        if (block == &func.getEntryBlock()) {
          LowerArguments(hasVAStart);

          // Set the stack size of the new function.
          auto &MFI = MF->getFrameInfo();
          for (auto &object : func.objects()) {
            auto index = MFI.CreateStackObject(
                object.Size,
                llvm::Align(object.Alignment),
                false
            );
            stackIndices_.insert({ object.Index, index });
          }
        }

        // Define incoming registers to landing pads.
        if (block->IsLandingPad()) {
          assert(block->pred_size() == 1 && "landing pad with multiple preds");
          auto *pred = *block->pred_begin();
          auto *call = ::cast_or_null<const InvokeInst>(pred->GetTerminator());
          assert(call && "landing pat does not follow invoke");
        }

        // Set up the SelectionDAG for the block.
        for (const auto &inst : *block) {
          Lower(&inst);
        }
      }

      // Ensure all values were exported.
      assert(!HasPendingExports() && "not all values were exported");

      // Lower the block.
      insert_ = MBB_->end();
      CodeGenAndEmitDAG();

      // Assertion to ensure that frames follow calls.
      for (auto it = MBB_->rbegin(); it != MBB_->rend(); it++) {
        if (it->isGCRoot() || it->isGCCall()) {
          auto call = std::next(it);
          assert(call != MBB_->rend() && call->isCall() && "invalid frame");
        }
      }

      // Clear values, except exported ones.
      values_.clear();
    }

    // If the entry block has a predecessor, insert a dummy entry.
    llvm::MachineBasicBlock *entryMBB = mbbs_[&func.getEntryBlock()];
    if (entryMBB->pred_size() != 0) {
      MBB_ = MF->CreateMachineBasicBlock();
      DAG.setRoot(DAG.getNode(
          ISD::BR,
          SDL_,
          MVT::Other,
          DAG.getRoot(),
          DAG.getBasicBlock(entryMBB)
      ));

      insert_ = MBB_->end();
      CodeGenAndEmitDAG();

      MF->push_front(MBB_);
      MBB_->addSuccessor(entryMBB);
      entryMBB = MBB_;
    }

    // Emit copies from args into vregs at the entry.
    MRI.EmitLiveInCopies(entryMBB, TRI, TII);
    TLI.finalizeLowering(*MF);

    MF->verify(nullptr, "LLIR-to-X86 ISel");

    MBB_ = nullptr;
    MF = nullptr;
  }

  // Finalize lowering of references.
  for (const auto &data : prog_.data()) {
    for (const Object &object : data) {
      for (const Atom &atom : object) {
        for (const Item &item : atom) {
          if (item.GetKind() != Item::Kind::EXPR) {
            continue;
          }

          auto *expr = item.GetExpr();
          switch (expr->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto *offsetExpr = static_cast<SymbolOffsetExpr *>(expr);
              if (auto *block = ::cast_or_null<Block>(offsetExpr->GetSymbol())) {
                auto *MBB = mbbs_[block];
                auto *BB = bbs_[block];
                MBB->setHasAddressTaken();
                llvm::BlockAddress::get(BB->getParent(), BB);
              }
              break;
            }
          }
        }
      }
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
static llvm::CallingConv::ID getLLVMCallingConv(CallingConv conv)
{
  switch (conv) {
    case CallingConv::C:          return llvm::CallingConv::C;
    case CallingConv::CAML:       return llvm::CallingConv::LLIR_CAML;
    case CallingConv::SETJMP:     return llvm::CallingConv::LLIR_SETJMP;
    case CallingConv::CAML_ALLOC: return llvm::CallingConv::LLIR_CAML_ALLOC;
    case CallingConv::CAML_GC:    return llvm::CallingConv::LLIR_CAML_GC;
    case CallingConv::XEN:        return llvm::CallingConv::LLIR_XEN;
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
static std::tuple<GlobalValue::LinkageTypes, GlobalValue::VisibilityTypes, bool>
getLLVMVisibility(Visibility vis)
{
  switch (vis) {
    case Visibility::LOCAL: {
      return { GlobalValue::InternalLinkage, GlobalValue::DefaultVisibility, true };
    }
    case Visibility::GLOBAL_DEFAULT: {
      return { GlobalValue::ExternalLinkage, GlobalValue::DefaultVisibility, false };
    }
    case Visibility::GLOBAL_HIDDEN: {
      return { GlobalValue::ExternalLinkage, GlobalValue::HiddenVisibility, true };
    }
    case Visibility::WEAK_DEFAULT: {
      return { GlobalValue::WeakAnyLinkage, GlobalValue::DefaultVisibility, false };
    }
    case Visibility::WEAK_HIDDEN: {
      return { GlobalValue::WeakAnyLinkage, GlobalValue::HiddenVisibility, true };
    }
  }
  llvm_unreachable("invalid visibility");
};

// -----------------------------------------------------------------------------
void ISel::PrepareGlobals()
{
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  auto &Ctx = M_->getContext();

  voidTy_ = llvm::Type::getVoidTy(Ctx);
  i8PtrTy_ = llvm::Type::getInt1PtrTy (Ctx);
  funcTy_ = llvm::FunctionType::get(voidTy_, {});

  // Create function definitions for all functions.
  for (const Func &func : prog_) {
    // Determine the LLVM linkage type.
    auto [linkage, visibility, dso] = getLLVMVisibility(func.GetVisibility());

    // Add a dummy function to the module.
    auto *F = llvm::Function::Create(funcTy_, linkage, 0, func.getName(), M_);
    F->setVisibility(visibility);
    F->setDSOLocal(dso);

    // Forward target features if set.
    auto features = func.getFeatures();
    if (!features.empty()) {
      F->addFnAttr("target-features", features);
    }

    // Set a dummy calling conv to emulate the set
    // of registers preserved by the callee.
    F->setCallingConv(getLLVMCallingConv(func.GetCallingConv()));
    F->setDoesNotThrow();
    llvm::BasicBlock* block = llvm::BasicBlock::Create(F->getContext(), "entry", F);
    llvm::IRBuilder<> builder(block);
    builder.CreateRetVoid();

    // Create MBBs for each block.
    auto *MF = &MMI.getOrCreateMachineFunction(*F);
    PrepareFunction(func, *MF);
    funcs_[&func] = MF;
    for (const Block &block : func) {
      // Create a skeleton basic block, with a jump to itself.
      llvm::BasicBlock *BB = llvm::BasicBlock::Create(
          M_->getContext(),
          block.getName(),
          F,
          nullptr
      );
      llvm::BranchInst::Create(BB, BB);
      bbs_[&block] = BB;

      // Create the basic block to be filled in by the instruction selector.
      llvm::MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(BB);
      mbbs_[&block] = MBB;
      MF->push_back(MBB);
    }
  }

  // Create objects for all atoms.
  for (const auto &data : prog_.data()) {
    for (const Object &object : data) {
      for (const Atom &atom : object) {
        // Determine the LLVM linkage type.
        auto [linkage, visibility, dso] = getLLVMVisibility(atom.GetVisibility());

        auto *GV = new llvm::GlobalVariable(
            *M_,
            i8PtrTy_,
            false,
            linkage,
            nullptr,
            atom.getName()
        );
        GV->setVisibility(visibility);
        GV->setDSOLocal(dso);
      }
    }
  }

  // Create function declarations for externals.
  for (const Extern &ext : prog_.externs()) {
    auto [linkage, visibility, dso] = getLLVMVisibility(ext.GetVisibility());
    llvm::GlobalObject *GV = nullptr;
    if (ext.GetSection() == ".text") {
      auto C = M_->getOrInsertFunction(ext.getName(), funcTy_);
      GV = llvm::cast<llvm::Function>(C.getCallee());
      GV->setDSOLocal(true);
    } else if (ext.GetName() == "caml_call_gc") {
      GV = llvm::Function::Create(
          funcTy_,
          llvm::GlobalValue::ExternalLinkage,
          0,
          ext.getName(),
          M_
      );
      GV->setDSOLocal(dso);
    } else {
      GV = new llvm::GlobalVariable(
          *M_,
          i8PtrTy_,
          false,
          linkage,
          nullptr,
          ext.getName(),
          nullptr,
          llvm::GlobalVariable::NotThreadLocal,
          0,
          true
      );
      GV->setDSOLocal(dso);
    }
    GV->setVisibility(visibility);
  }
}

// -----------------------------------------------------------------------------
void ISel::Lower(const Inst *i)
{
  if (i->IsTerminator()) {
    HandleSuccessorPHI(i->getParent());
  }

  switch (i->GetKind()) {
    // Nodes handled separately.
    case Inst::Kind::PHI:
    case Inst::Kind::ARG:
      return;
    // Target-specific instructions.
    case Inst::Kind::X86_XCHG:
    case Inst::Kind::X86_CMPXCHG:
    case Inst::Kind::X86_FNSTCW:
    case Inst::Kind::X86_FNSTSW:
    case Inst::Kind::X86_FNSTENV:
    case Inst::Kind::X86_FLDCW:
    case Inst::Kind::X86_FLDENV:
    case Inst::Kind::X86_LDMXCSR:
    case Inst::Kind::X86_STMXCSR:
    case Inst::Kind::X86_FNCLEX:
    case Inst::Kind::X86_RDTSC:
    case Inst::Kind::AARCH64_LL:
    case Inst::Kind::AARCH64_SC:
    case Inst::Kind::AARCH64_DMB:
    case Inst::Kind::RISCV_XCHG:
    case Inst::Kind::RISCV_CMPXCHG:
    case Inst::Kind::RISCV_FENCE:
    case Inst::Kind::RISCV_GP:
    case Inst::Kind::PPC_LL:
    case Inst::Kind::PPC_SC:
    case Inst::Kind::PPC_SYNC:
    case Inst::Kind::PPC_ISYNC:
      return LowerArch(i);
    // Control flow.
    case Inst::Kind::CALL:        return LowerCall(static_cast<const CallInst *>(i));
    case Inst::Kind::TCALL:       return LowerTailCall(static_cast<const TailCallInst *>(i));
    case Inst::Kind::INVOKE:      return LowerInvoke(static_cast<const InvokeInst *>(i));
    case Inst::Kind::RET:         return LowerReturn(static_cast<const ReturnInst *>(i));
    case Inst::Kind::JCC:         return LowerJCC(static_cast<const JumpCondInst *>(i));
    case Inst::Kind::RAISE:       return LowerRaise(static_cast<const RaiseInst *>(i));
    case Inst::Kind::LANDING_PAD: return LowerLandingPad(static_cast<const LandingPadInst *>(i));
    case Inst::Kind::JMP:         return LowerJMP(static_cast<const JumpInst *>(i));
    case Inst::Kind::SWITCH:      return LowerSwitch(static_cast<const SwitchInst *>(i));
    case Inst::Kind::TRAP:        return LowerTrap(static_cast<const TrapInst *>(i));
    // Memory.
    case Inst::Kind::LD:       return LowerLD(static_cast<const LoadInst *>(i));
    case Inst::Kind::ST:       return LowerST(static_cast<const StoreInst *>(i));
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
    case Inst::Kind::BSWAP:    return LowerUnary(static_cast<const UnaryInst *>(i), ISD::BSWAP);
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
    case Inst::Kind::XOR:      return LowerBinary(i, ISD::XOR);
    case Inst::Kind::SLL:      return LowerShift(i, ISD::SHL);
    case Inst::Kind::SRA:      return LowerShift(i, ISD::SRA);
    case Inst::Kind::SRL:      return LowerShift(i, ISD::SRL);
    case Inst::Kind::ROTL:     return LowerShift(i, ISD::ROTL);
    case Inst::Kind::ROTR:     return LowerShift(i, ISD::ROTR);
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
    // Target-specific generics.
    case Inst::Kind::SET:      return LowerSet(static_cast<const SetInst *>(i));
    case Inst::Kind::SYSCALL:  return LowerSyscall(static_cast<const SyscallInst *>(i));
    case Inst::Kind::CLONE:    return LowerClone(static_cast<const CloneInst *>(i));
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetValue(ConstRef<Inst> inst)
{
  if (auto vt = values_.find(inst); vt != values_.end()) {
    return vt->second;
  }

  if (auto rt = regs_.find(inst); rt != regs_.end()) {
    auto &DAG = GetDAG();
    auto &Ctx = *DAG.getContext();

    llvm::SmallVector<SDValue, 2> parts;
    for (auto &[reg, regVT] : rt->second) {
      parts.push_back(DAG.getCopyFromReg(
          DAG.getEntryNode(),
          SDL_,
          reg,
          regVT
      ));
    }

    MVT vt = GetVT(inst.GetType());
    switch (parts.size()) {
      default: case 0: {
        llvm_unreachable("invalid partition");
      }
      case 1: {
        SDValue ret = parts[0];
        if (vt != ret.getSimpleValueType()) {
          ret = DAG.getAnyExtOrTrunc(ret, SDL_, vt);
        }
        return ret;
      }
      case 2: {
        return DAG.getNode(
            ISD::BUILD_PAIR,
            SDL_,
            vt,
            parts[0],
            parts[1]
        );
      }
    }
  } else {
    return LowerConstant(inst);
  }
}

// -----------------------------------------------------------------------------
void ISel::Export(ConstRef<Inst> inst, SDValue value)
{
  values_[inst] = value;
  auto it = regs_.find(inst);
  if (it != regs_.end()) {
    if (inst.GetType() == Type::V64) {
      pendingValueInsts_.emplace(inst, it->second);
    } else {
      pendingPrimInsts_.emplace(inst, it->second);
    }
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LoadReg(ConstantReg::Kind reg)
{
  auto &DAG = GetDAG();
  auto &MFI = DAG.getMachineFunction().getFrameInfo();
  auto ptrVT = DAG.getTargetLoweringInfo().getPointerTy(DAG.getDataLayout());

  switch (reg) {
    // Stack pointer.
    case ConstantReg::Kind::SP: {
      auto node = DAG.getNode(
          ISD::STACKSAVE,
          SDL_,
          DAG.getVTList(ptrVT, MVT::Other),
          DAG.getRoot()
      );
      DAG.setRoot(node.getValue(1));
      return node.getValue(0);
    }
    // Return address.
    case ConstantReg::Kind::RET_ADDR: {
      return DAG.getNode(
          ISD::RETURNADDR,
          SDL_,
          ptrVT,
          DAG.getIntPtrConstant(0, SDL_)
      );
    }
    // Frame address.
    case ConstantReg::Kind::FRAME_ADDR: {
      MFI.setReturnAddressIsTaken(true);
      if (frameIndex_ == 0) {
        frameIndex_ = MFI.CreateFixedObject(8, 0, false);
      }
      return DAG.getFrameIndex(frameIndex_, ptrVT);
    }
    // Loads an architecture-specific register.
    case ConstantReg::Kind::FS:
    case ConstantReg::Kind::AARCH64_FPSR:
    case ConstantReg::Kind::AARCH64_FPCR:
    case ConstantReg::Kind::RISCV_FFLAGS:
    case ConstantReg::Kind::RISCV_FRM:
    case ConstantReg::Kind::RISCV_FCSR:
    case ConstantReg::Kind::PPC_FPSCR: {
      return LoadRegArch(reg);
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerGlobal(const Global &val)
{
  auto &DAG = GetDAG();
  auto ptrVT = DAG.getTargetLoweringInfo().getPointerTy(DAG.getDataLayout());

  switch (val.GetKind()) {
    case Global::Kind::BLOCK: {
      auto &block = static_cast<const Block &>(val);
      if (auto *MBB = mbbs_[&block]) {
        auto *BB = const_cast<llvm::BasicBlock *>(MBB->getBasicBlock());
        auto *BA = llvm::BlockAddress::get(F_, BB);
        MBB->setHasAddressTaken();
        return DAG.getBlockAddress(BA, ptrVT);
      } else {
        llvm::report_fatal_error("Unknown block '" + val.getName() + "'");
      }
    }
    case Global::Kind::FUNC:
    case Global::Kind::ATOM:
    case Global::Kind::EXTERN: {
      // Atom reference - need indirection for shared objects.
      auto *GV = M_->getNamedValue(val.getName());
      if (!GV) {
        llvm::report_fatal_error("Unknown symbol '" + val.getName() + "'");
        break;
      }

      return DAG.getGlobalAddress(GV, SDL_, ptrVT);
    }
  }
  llvm_unreachable("invalid global type");
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerGlobal(const Global &val, int64_t offset)
{
  if (offset == 0) {
    return LowerGlobal(val);
  } else {
    auto &DAG = GetDAG();
    auto ptrVT = DAG.getTargetLoweringInfo().getPointerTy(DAG.getDataLayout());
    return DAG.getNode(
        ISD::ADD,
        SDL_,
        ptrVT,
        LowerGlobal(val),
        DAG.getConstant(offset, SDL_, ptrVT)
    );
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerArgs(const CallLowering &lowering)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  for (auto &argLoc : lowering.args()) {
    llvm::SmallVector<SDValue, 2> parts;
    for (auto &part : argLoc.Parts) {
      switch (part.K) {
        case CallLowering::ArgPart::Kind::REG: {
          auto regClass = TLI.getRegClassFor(part.VT);
          auto reg = MF.addLiveIn(part.Reg, regClass);
          parts.push_back(DAG.getCopyFromReg(
              DAG.getEntryNode(),
              SDL_,
              reg,
              part.VT
          ));
          continue;
        }
        case CallLowering::ArgPart::Kind::STK: {
          llvm::MachineFrameInfo &MFI = MF.getFrameInfo();
          int index = MFI.CreateFixedObject(part.Size, part.Offset, true);
          parts.push_back(DAG.getLoad(
              part.VT,
              SDL_,
              DAG.getEntryNode(),
              DAG.getFrameIndex(index, TLI.getPointerTy(DAG.getDataLayout())),
              llvm::MachinePointerInfo::getFixedStack(
                  MF,
                  index
              )
          ));
          continue;
        }
      }
      llvm_unreachable("invalid argument part");
    }

    SDValue arg;
    MVT vt = GetVT(argLoc.ArgType);
    switch (parts.size()) {
      default: case 0: {
        llvm_unreachable("invalid partition");
      }
      case 1: {
        arg = parts[0];
        if (vt != arg.getSimpleValueType()) {
          arg = DAG.getAnyExtOrTrunc(arg, SDL_, vt);
        }
        break;
      }
      case 2: {
        arg = DAG.getNode(
            ISD::BUILD_PAIR,
            SDL_,
            vt,
            parts[0],
            parts[1]
        );
        break;
      }
    }

    for (const auto &block : *func_) {
      for (const auto &inst : block) {
        if (auto *argInst = ::cast_or_null<const ArgInst>(&inst)) {
          if (argInst->GetIdx() == argLoc.Index) {
            Export(argInst, arg);
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::pair<llvm::SDValue, llvm::SDValue>
ISel::LowerRets(
    llvm::SDValue chain,
    const CallLowering &ci,
    const ReturnInst *inst,
    llvm::SmallVectorImpl<SDValue> &ops)
{
  auto &DAG = GetDAG();

  SDValue glue;
  for (unsigned i = 0, n = inst->arg_size(); i < n; ++i) {
    ConstRef<Inst> arg = inst->arg(i);
    SDValue fullValue = GetValue(arg);
    const MVT argVT = GetVT(arg.GetType());
    const CallLowering::RetLoc &ret = ci.Return(i);
    for (unsigned j = 0, m = ret.Parts.size(); j < m; ++j) {
      auto &part = ret.Parts[j];

      SDValue argValue;
      if (m == 1) {
        if (argVT != part.VT) {
          argValue = DAG.getAnyExtOrTrunc(fullValue, SDL_, part.VT);
        } else {
          argValue = fullValue;
        }
      } else {
        argValue = DAG.getNode(
            ISD::EXTRACT_ELEMENT,
            SDL_,
            part.VT,
            fullValue,
            DAG.getConstant(j, SDL_, part.VT)
        );
      }

      chain = DAG.getCopyToReg(chain, SDL_, part.Reg, argValue, glue);
      ops.push_back(DAG.getRegister(part.Reg, part.VT));
      glue = chain.getValue(1);
    }
  }

  return { chain, glue };
}

// -----------------------------------------------------------------------------
std::pair<llvm::SDValue, llvm::SDValue>
ISel::LowerRaises(
    llvm::SDValue chain,
    const CallLowering &ci,
    const RaiseInst *inst,
    llvm::SmallVectorImpl<llvm::Register> &regs,
    llvm::SDValue glue)
{
  auto &DAG = GetDAG();

  for (unsigned i = 0, n = inst->arg_size(); i < n; ++i) {
    ConstRef<Inst> arg = inst->arg(i);
    SDValue fullValue = GetValue(arg);
    const MVT argVT = GetVT(arg.GetType());
    const CallLowering::RetLoc &ret = ci.Return(i);
    for (unsigned j = 0, m = ret.Parts.size(); j < m; ++j) {
      auto &part = ret.Parts[j];

      SDValue argValue;
      if (m == 1) {
        if (argVT != part.VT) {
          argValue = DAG.getAnyExtOrTrunc(fullValue, SDL_, part.VT);
        } else {
          argValue = fullValue;
        }
      } else {
        argValue = DAG.getNode(
            ISD::EXTRACT_ELEMENT,
            SDL_,
            part.VT,
            fullValue,
            DAG.getConstant(j, SDL_, part.VT)
        );
      }

      chain = DAG.getCopyToReg(chain, SDL_, part.Reg, argValue, glue);
      regs.push_back(part.Reg);
      glue = chain.getValue(1);
    }
  }

  return { chain, glue };
}

// -----------------------------------------------------------------------------
void ISel::LowerPad(const CallLowering &ci, const LandingPadInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  for (unsigned i = 0, n = inst->type_size(); i < n; ++i) {
    auto &retLoc = ci.Return(i);
    llvm::SmallVector<SDValue, 2> parts;
    for (auto &part : retLoc.Parts) {
      auto regClass = TLI.getRegClassFor(part.VT);
      auto reg = MBB_->addLiveIn(part.Reg, regClass);
      parts.push_back(DAG.getCopyFromReg(
          DAG.getEntryNode(),
          SDL_,
          reg,
          part.VT
      ));
    }

    SDValue ret;
    MVT vt = GetVT(inst->type(retLoc.Index));
    switch (parts.size()) {
      default: case 0: {
        llvm_unreachable("invalid partition");
      }
      case 1: {
        ret = parts[0];
        if (vt != ret.getSimpleValueType()) {
          ret = DAG.getAnyExtOrTrunc(ret, SDL_, vt);
        }
        break;
      }
      case 2: {
        ret = DAG.getNode(
            ISD::BUILD_PAIR,
            SDL_,
            vt,
            parts[0],
            parts[1]
        );
        break;
      }
    }
    Export(inst->GetSubValue(retLoc.Index), ret);
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetPrimitiveExportRoot()
{
  ExportList exports;
  for (auto &[reg, value] : pendingPrimValues_) {
    exports.emplace_back(reg, value);
  }
  for (const auto &[inst, reg] : pendingPrimInsts_) {
    auto it = values_.find(inst);
    assert(it != values_.end() && "value not defined");
    exports.emplace_back(reg, it->second);
  }
  pendingPrimValues_.clear();
  pendingPrimInsts_.clear();
  return GetExportRoot(exports);
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetValueExportRoot()
{
  ExportList exports;
  for (auto &[inst, reg] : pendingValueInsts_) {
    auto it = values_.find(inst);
    assert(it != values_.end() && "value not defined");
    exports.emplace_back(reg, it->second);
  }
  pendingValueInsts_.clear();
  return GetExportRoot(exports);
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetExportRoot()
{
  ExportList exports;
  for (auto &[regs, value] : pendingPrimValues_) {
    exports.emplace_back(regs, value);
  }
  for (auto &[inst, reg] : pendingPrimInsts_) {
    auto it = values_.find(inst);
    assert(it != values_.end() && "value not defined");
    exports.emplace_back(reg, it->second);
  }
  for (auto &[inst, reg] : pendingValueInsts_) {
    auto it = values_.find(inst);
    assert(it != values_.end() && "value not defined");
    exports.emplace_back(reg, it->second);
  }
  pendingPrimValues_.clear();
  pendingPrimInsts_.clear();
  pendingValueInsts_.clear();
  return GetExportRoot(exports);
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::GetExportRoot(const ExportList &exports)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &Ctx = *DAG.getContext();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  SDValue root = DAG.getRoot();
  if (exports.empty()) {
    return root;
  }

  bool exportsRoot = false;
  llvm::SmallVector<llvm::SDValue, 8> chains;
  for (auto &[regs, value] : exports) {
    MVT valVT = value.getSimpleValueType();
    for (unsigned i = 0, n = regs.size(); i < n; ++i) {
      auto &[reg, regVT] = regs[i];

      SDValue part;
      if (n == 1) {
        if (valVT == regVT) {
          part = value;
        } else {
          part = DAG.getAnyExtOrTrunc(value, SDL_, regVT);
        }
      } else {
        part = DAG.getNode(
            ISD::EXTRACT_ELEMENT,
            SDL_,
            regVT,
            value,
            DAG.getConstant(i, SDL_, regVT)
        );
      }

      chains.push_back(DAG.getCopyToReg(DAG.getEntryNode(), SDL_, reg, part));
    }
    auto *node = value.getNode();
    if (node->getNumOperands() > 0 && node->getOperand(0) == root) {
      exportsRoot = true;
    }
  }

  if (root.getOpcode() != ISD::EntryToken && !exportsRoot) {
    chains.push_back(root);
  }

  SDValue factor = DAG.getNode(
      ISD::TokenFactor,
      SDL_,
      MVT::Other,
      chains
  );
  DAG.setRoot(factor);
  return factor;
}

// -----------------------------------------------------------------------------
bool ISel::HasPendingExports()
{
  if (!pendingPrimValues_.empty()) {
    return true;
  }
  if (!pendingPrimInsts_.empty()) {
    return true;
  }
  if (!pendingValueInsts_.empty()) {
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
ISel::RegParts ISel::AssignVReg(ConstRef<Inst> inst)
{
  auto &DAG = GetDAG();
  auto &Ctx = *DAG.getContext();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  // Find the register type & class which can hold this argument, along
  // with the required number of distinct registers, reserving them.
  MVT valVT = GetVT(inst.GetType());
  MVT regVT = TLI.getRegisterType(Ctx, valVT);
  auto regClass = TLI.getRegClassFor(regVT);
  unsigned numRegs = TLI.getNumRegisters(Ctx, valVT);

  RegParts regs;
  for (unsigned i = 0; i < numRegs; ++i) {
    regs.emplace_back(MRI.createVirtualRegister(regClass), regVT);
  }

  regs_[inst] = regs;
  return regs;
}

// -----------------------------------------------------------------------------
ISel::RegParts ISel::ExportValue(llvm::SDValue value)
{
  auto &DAG = GetDAG();
  auto &Ctx = *DAG.getContext();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  MVT valVT = value.getSimpleValueType();
  MVT regVT = TLI.getRegisterType(Ctx, valVT);
  auto regClass = TLI.getRegClassFor(regVT);
  unsigned numRegs = TLI.getNumRegisters(Ctx, valVT);

  RegParts regs;
  for (unsigned i = 0; i < numRegs; ++i) {
    regs.emplace_back(MRI.createVirtualRegister(regClass), regVT);
  }
  pendingPrimValues_.emplace_back(regs, value);
  return regs;
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerInlineAsm(
    unsigned opcode,
    SDValue chain,
    const char *code,
    unsigned flags,
    llvm::ArrayRef<llvm::Register> inputs,
    llvm::ArrayRef<llvm::Register> clobbers,
    llvm::ArrayRef<llvm::Register> outputs,
    SDValue glue)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  // Set up the inline assembly node.
  llvm::SmallVector<SDValue, 7> ops;
  ops.push_back(chain);
  ops.push_back(DAG.getTargetExternalSymbol(
      code,
      TLI.getProgramPointerTy(DAG.getDataLayout())
  ));
  ops.push_back(DAG.getMDNode(nullptr));
  ops.push_back(DAG.getTargetConstant(
      flags,
      SDL_,
      TLI.getPointerTy(DAG.getDataLayout())
  ));

  // Find the flag for a register.
  auto GetFlag = [&](unsigned kind, llvm::Register reg) -> unsigned
  {
    if (llvm::Register::isVirtualRegister(reg)) {
      const auto *RC = MRI.getRegClass(reg);
      return llvm::InlineAsm::getFlagWordForRegClass(
          llvm::InlineAsm::getFlagWord(kind, 1),
          RC->getID()
      );
    } else {
      return llvm::InlineAsm::getFlagWord(kind, 1);
    }
  };

  // Register the output.
  {
    unsigned flag = llvm::InlineAsm::getFlagWord(
        llvm::InlineAsm::Kind_RegDef, 1
    );
    for (llvm::Register reg : outputs) {
      unsigned flag = GetFlag(llvm::InlineAsm::Kind_RegDef, reg);
      ops.push_back(DAG.getTargetConstant(flag, SDL_, MVT::i32));
      ops.push_back(DAG.getRegister(reg, MVT::i64));
    }
  }

  // Register the input.
  {
    for (llvm::Register reg : inputs) {
      unsigned flag = GetFlag(llvm::InlineAsm::Kind_RegUse, reg);
      ops.push_back(DAG.getTargetConstant(flag, SDL_, MVT::i32));
      ops.push_back(DAG.getRegister(reg, MVT::i32));
    }
  }

  // Register clobbers.
  {
    unsigned flag = llvm::InlineAsm::getFlagWord(
        llvm::InlineAsm::Kind_Clobber, 1
    );
    for (llvm::Register clobber : clobbers) {
      ops.push_back(DAG.getTargetConstant(flag, SDL_, MVT::i32));
      ops.push_back(DAG.getRegister(clobber, MVT::i32));
    }
  }

  // Add the glue.
  if (glue) {
    ops.push_back(glue);
  }

  // Create the inlineasm node.
  return DAG.getNode(opcode, SDL_, DAG.getVTList(MVT::Other, MVT::Glue), ops);
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
    case Type::V64:
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
    case Type::F128: {
      U u { .i = val.getSExtValue() };
      return GetDAG().getConstantFP(u.d, SDL_, MVT::f128);
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
    case Type::V64:
    case Type::I128:
      llvm_unreachable("not supported");
    case Type::F32:
      return GetDAG().getConstantFP(val, SDL_, MVT::f32);
    case Type::F64:
      return GetDAG().getConstantFP(val, SDL_, MVT::f64);
    case Type::F80:
      return GetDAG().getConstantFP(val, SDL_, MVT::f80);
    case Type::F128:
      return GetDAG().getConstantFP(val, SDL_, MVT::f128);
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerConstant(ConstRef<Inst> inst)
{
  if (ConstRef<MovInst> movInst = ::cast_or_null<MovInst>(inst)) {
    Type rt = movInst->GetType();
    switch (ConstRef<Value> val = GetMoveArg(movInst); val->GetKind()) {
      case Value::Kind::INST: {
        Error(inst.Get(), "not a constant");
      }
      case Value::Kind::CONST: {
        const Constant &constVal = *::cast_or_null<Constant>(val);
        switch (constVal.GetKind()) {
          case Constant::Kind::REG: {
            Error(inst.Get(), "not a constant");
          }
          case Constant::Kind::INT: {
            auto &constInst = static_cast<const ConstantInt &>(constVal);
            return LowerImm(constInst.GetValue(), rt);
          }
          case Constant::Kind::FLOAT: {
            auto &constFloat = static_cast<const ConstantFloat &>(constVal);
            return LowerImm(constFloat.GetValue(), rt);
          }
        }
        llvm_unreachable("invalid constant kind");
      }
      case Value::Kind::GLOBAL: {
        if (!IsPointerType(movInst->GetType())) {
          Error(movInst.Get(), "Invalid address type");
        }
        return LowerGlobal(*::cast_or_null<Global>(val), 0);
      }
      case Value::Kind::EXPR: {
        if (!IsPointerType(movInst->GetType())) {
          Error(movInst.Get(), "Invalid address type");
        }
        return LowerExpr(*::cast_or_null<Expr>(val));
      }
    }
    llvm_unreachable("invalid value kind");
  } else {
    Error(inst.Get(), "not a move instruction");
  }
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerExpr(const Expr &expr)
{
  switch (expr.GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto &symOff = static_cast<const SymbolOffsetExpr &>(expr);
      return LowerGlobal(*symOff.GetSymbol(), symOff.GetOffset());
    }
  }
  llvm_unreachable("invalid expression");
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
    case Cond::O:   return ISD::CondCode::SETO;
    case Cond::OEQ: return ISD::CondCode::SETOEQ;
    case Cond::ONE: return ISD::CondCode::SETONE;
    case Cond::OLE: return ISD::CondCode::SETOLE;
    case Cond::OLT: return ISD::CondCode::SETOLT;
    case Cond::OGE: return ISD::CondCode::SETOGE;
    case Cond::OGT: return ISD::CondCode::SETOGT;
    case Cond::UO:  return ISD::CondCode::SETUO;
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
ISel::FrameExports ISel::GetFrameExport(const Inst *frame)
{
  if (!lva_) {
    lva_.reset(new LiveVariables(func_));
  }

  auto &dag = GetDAG();
  auto &mf = dag.getMachineFunction();

  FrameExports exports;
  for (ConstRef<Inst> inst : lva_->LiveOut(frame)) {
    if (inst.GetType() != Type::V64) {
      continue;
    }
    if (inst.Get() == frame) {
      continue;
    }
    // Constant values might be tagged as such, but are not GC roots.
    SDValue v = GetValue(inst);
    if (llvm::isa<llvm::GlobalAddressSDNode>(v)) {
      continue;
    }
    if (llvm::isa<llvm::ConstantSDNode>(v)) {
      continue;
    }
    exports.emplace_back(inst, v);
  }
  return exports;
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerGCFrame(
    SDValue chain,
    SDValue glue,
    const CallSite *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MMI = MF.getMMI();
  auto &STI = MF.getSubtarget();
  auto &TRI = *STI.getRegisterInfo();
  auto &TLI = *STI.getTargetLowering();

  const Func *func = inst->getParent()->getParent();

  auto *symbol = MMI.getContext().createTempSymbol();
  const auto *frame =  inst->template GetAnnot<CamlFrame>();
  frames_[symbol] = frame;

  ISD::FrameType op = ISD::ROOT;
  switch (inst->GetCallingConv()) {
    case CallingConv::C: {
      op = ISD::CALL;
      break;
    }
    case CallingConv::CAML: {
      if (func->GetCallingConv() == CallingConv::C) {
        op = ISD::ROOT;
      } else {
        op = ISD::CALL;
      }
      break;
    }
    case CallingConv::CAML_ALLOC:
    case CallingConv::CAML_GC: {
      op = ISD::ALLOC;
      break;
    }
    case CallingConv::SETJMP:
    case CallingConv::XEN: {
      llvm_unreachable("invalid frame");
    }
  }

  llvm::SmallVector<SDValue, 8> frameOps;
  frameOps.push_back(chain);
  for (auto &[inst, val] : GetFrameExport(inst)) {
    frameOps.push_back(val);
  }
  frameOps.push_back(glue);

  SDVTList frameTypes = DAG.getVTList(MVT::Other, MVT::Glue);
  return DAG.getGCFrame(SDL_, op, frameTypes, frameOps, symbol);
}

// -----------------------------------------------------------------------------
ConstRef<Value> ISel::GetMoveArg(ConstRef<MovInst> inst)
{
  if (ConstRef<MovInst> arg = ::cast_or_null<MovInst>(inst->GetArg())) {
    if (!CompatibleType(arg->GetType(), inst->GetType())) {
      return arg;
    }
    return GetMoveArg(arg);
  }
  return inst->GetArg();
}

// -----------------------------------------------------------------------------
bool ISel::IsExported(ConstRef<Inst> inst)
{
  if (inst->use_empty()) {
    return false;
  }
  if (inst->Is(Inst::Kind::PHI)) {
    return true;
  }

  if (ConstRef<MovInst> movInst = ::cast_or_null<MovInst>(inst)) {
    ConstRef<Value> val = GetMoveArg(movInst);
    switch (val->GetKind()) {
      case Value::Kind::INST: {
        break;
      }
      case Value::Kind::CONST: {
        const Constant &constVal = *::cast_or_null<Constant>(val);
        switch (constVal.GetKind()) {
          case Constant::Kind::REG: {
            break;
          }
          case Constant::Kind::INT:
          case Constant::Kind::FLOAT: {
            return false;
          }
        }
        break;
      }
      case Value::Kind::GLOBAL:
      case Value::Kind::EXPR: {
        return false;
      }
    }
  }

  return UsedOutside(inst, inst->getParent());
}

// -----------------------------------------------------------------------------
std::pair<bool, llvm::CallingConv::ID>
ISel::GetCallingConv(const Func *caller, const CallSite *call)
{
  bool needsTrampoline = false;
  if (caller->GetCallingConv() == CallingConv::CAML) {
    switch (call->GetCallingConv()) {
      case CallingConv::C: {
        needsTrampoline = call->HasAnnot<CamlFrame>();
        break;
      }
      case CallingConv::SETJMP:
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC:
      case CallingConv::CAML_GC:
      case CallingConv::XEN: {
        break;
      }
    }
  }

  // Find the register mask, based on the calling convention.
  using namespace llvm::CallingConv;
  if (needsTrampoline) {
    return { true, LLIR_CAML_EXT_INVOKE };
  }
  llvm::CallingConv::ID cc;
  switch (call->GetCallingConv()) {
    case CallingConv::C:          return { false, C };
    case CallingConv::CAML:       return { false, LLIR_CAML };
    case CallingConv::CAML_ALLOC: return { false, LLIR_CAML_ALLOC };
    case CallingConv::CAML_GC:    return { false, LLIR_CAML_GC };
    case CallingConv::SETJMP:     return { false, LLIR_SETJMP };
    case CallingConv::XEN:        return { false, LLIR_XEN };
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
void ISel::HandleSuccessorPHI(const Block *block)
{
  auto &DAG = GetDAG();
  auto &Ctx = *DAG.getContext();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  auto *blockMBB = mbbs_[block];
  llvm::SmallPtrSet<llvm::MachineBasicBlock *, 4> handled;
  for (const Block *succBB : block->successors()) {
    llvm::MachineBasicBlock *succMBB = mbbs_[succBB];
    if (!handled.insert(succMBB).second) {
      continue;
    }

    auto phiIt = succMBB->begin();
    for (const PhiInst &phi : succBB->phis()) {
      if (phi.use_empty()) {
        continue;
      }

      ConstRef<Inst> inst = phi.GetValue(block);
      Type phiType = phi.GetType();
      MVT phiVT = GetVT(phiType);
      MVT regVT = TLI.getRegisterType(Ctx, phiVT);
      auto regClass = TLI.getRegClassFor(regVT);

      RegParts regs;
      if (ConstRef<MovInst> movInst = ::cast_or_null<MovInst>(inst)) {
        ConstRef<Value> arg = GetMoveArg(movInst);
        switch (arg->GetKind()) {
          case Value::Kind::INST: {
            auto it = regs_.find(inst);
            if (it != regs_.end()) {
              regs = it->second;
            } else {
              regs = ExportValue(LowerConstant(inst));
            }
            break;
          }
          case Value::Kind::GLOBAL: {
            if (!IsPointerType(phi.GetType())) {
              Error(&phi, "Invalid address type");
            }
            regs = ExportValue(LowerGlobal(*::cast_or_null<Global>(arg), 0));
            break;
          }
          case Value::Kind::EXPR: {
            if (!IsPointerType(phi.GetType())) {
              Error(&phi, "Invalid address type");
            }
            regs = ExportValue(LowerExpr(*::cast_or_null<Expr>(arg)));
            break;
          }
          case Value::Kind::CONST: {
            const Constant &constVal = *::cast_or_null<Constant>(arg);
            switch (constVal.GetKind()) {
              case Constant::Kind::INT: {
                regs = ExportValue(LowerImm(
                    static_cast<const ConstantInt &>(constVal).GetValue(),
                    phiType
                ));
                break;
              }
              case Constant::Kind::FLOAT: {
                regs = ExportValue(LowerImm(
                    static_cast<const ConstantFloat &>(constVal).GetValue(),
                    phiType
                ));
                break;
              }
              case Constant::Kind::REG: {
                auto it = regs_.find(inst);
                if (it != regs_.end()) {
                  regs = it->second;
                } else {
                  Error(&phi, "Invalid incoming register to PHI.");
                }
                break;
              }
            }
            break;
          }
        }
      } else {
        auto it = regs_.find(inst);
        assert(it != regs_.end() && "missing vreg value");
        regs = it->second;
      }

      for (auto &[reg, regVT] : regs) {
        llvm::MachineInstrBuilder mPhi(DAG.getMachineFunction(), phiIt++);
        mPhi.addReg(reg).addMBB(blockMBB);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::CodeGenAndEmitDAG()
{
  bool changed;

  llvm::SelectionDAG &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  const auto &STI = MF.getSubtarget();
  const auto &TII = *STI.getInstrInfo();
  const auto &TRI = *STI.getRegisterInfo();
  const auto &TLI = *STI.getTargetLowering();

  llvm::AAResults *aa = nullptr;

  DAG.NewNodesMustHaveLegalTypes = false;
  DAG.Combine(llvm::BeforeLegalizeTypes, aa, ol_);
  changed = DAG.LegalizeTypes();
  DAG.NewNodesMustHaveLegalTypes = true;

  if (changed) {
    DAG.Combine(llvm::AfterLegalizeTypes, aa, ol_);
  }

  changed = DAG.LegalizeVectors();

  if (changed) {
    DAG.LegalizeTypes();
    DAG.Combine(llvm::AfterLegalizeVectorOps, aa, ol_);
  }

  DAG.Legalize();
  DAG.Combine(llvm::AfterLegalizeDAG, aa, ol_);

  DoInstructionSelection();

  llvm::ScheduleDAGSDNodes *Scheduler = createILPListDAGScheduler(
      &MF,
      &TII,
      &TRI,
      &TLI,
      ol_
  );

  Scheduler->Run(&DAG, MBB_);

  llvm::MachineBasicBlock *Fst = MBB_;
  MBB_ = Scheduler->EmitSchedule(insert_);
  llvm::MachineBasicBlock *Snd = MBB_;

  if (Fst != Snd) {
    llvm_unreachable("not implemented");
  }
  delete Scheduler;

  DAG.clear();
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
  llvm::SelectionDAG &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  PreprocessISelDAG();

  DAG.AssignTopologicalOrder();

  llvm::HandleSDNode dummy(DAG.getRoot());
  llvm::SelectionDAG::allnodes_iterator it(DAG.getRoot().getNode());
  ++it;

  ISelUpdater ISU(DAG, it);

  while (it != DAG.allnodes_begin()) {
    SDNode *node = &*--it;
    if (node->use_empty()) {
      continue;
    }

    if (!TLI.isStrictFPEnabled() && node->isStrictFPOpcode()) {
      EVT ActionVT;
      switch (node->getOpcode()) {
        case ISD::STRICT_SINT_TO_FP:
        case ISD::STRICT_UINT_TO_FP:
        case ISD::STRICT_LRINT:
        case ISD::STRICT_LLRINT:
        case ISD::STRICT_LROUND:
        case ISD::STRICT_LLROUND:
        case ISD::STRICT_FSETCC:
        case ISD::STRICT_FSETCCS: {
          ActionVT = node->getOperand(1).getValueType();
          break;
        }
        default: {
          ActionVT = node->getValueType(0);
          break;
        }
      }
      auto action = TLI.getOperationAction(node->getOpcode(), ActionVT);
      if (action == llvm::TargetLowering::Expand) {
        node = DAG.mutateStrictFPToFP(node);
      }
    }
    Select(node);
  }

  DAG.setRoot(dummy.getValue());

  PostprocessISelDAG();
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
void ISel::LowerVAStart(const VAStartInst *inst)
{
  if (!inst->getParent()->getParent()->IsVarArg()) {
    Error(inst, "vastart in a non-vararg function");
  }

  auto &DAG = GetDAG();
  DAG.setRoot(DAG.getNode(
      ISD::VASTART,
      SDL_,
      MVT::Other,
      DAG.getRoot(),
      GetValue(inst->GetVAList()),
      DAG.getSrcValue(nullptr)
  ));
}

// -----------------------------------------------------------------------------
void ISel::LowerCall(const CallInst *inst)
{
  auto &dag = GetDAG();

  // Find the continuation block.
  auto *sourceMBB = mbbs_[inst->getParent()];
  auto *contMBB = mbbs_[inst->GetCont()];

  // Lower the call.
  LowerCallSite(dag.getRoot(), inst);

  // Add a jump to the continuation block.
  dag.setRoot(dag.getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      dag.getBasicBlock(contMBB)
  ));

  // Mark successors.
  sourceMBB->addSuccessor(contMBB, BranchProbability::getOne());
}

// -----------------------------------------------------------------------------
void ISel::LowerTailCall(const TailCallInst *inst)
{
  LowerCallSite(GetDAG().getRoot(), inst);
}

// -----------------------------------------------------------------------------
void ISel::LowerInvoke(const InvokeInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();

  // Find the continuation blocks.
  auto &MMI = MF.getMMI();
  auto *bCont = inst->GetCont();
  auto *bThrow = inst->GetThrow();
  auto *mbbCont = mbbs_[bCont];
  auto *mbbThrow = mbbs_[bThrow];

  // Mark the landing pad as such.
  mbbThrow->setIsEHPad();

  // Lower the invoke call: export here since the call might not return.
  LowerCallSite(GetPrimitiveExportRoot(), inst);

  // Add a jump to the continuation block: export the invoke result.
  DAG.setRoot(DAG.getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      DAG.getBasicBlock(mbbCont)
  ));

  // Mark successors.
  auto *sourceMBB = mbbs_[inst->getParent()];
  sourceMBB->addSuccessor(mbbCont, BranchProbability::getOne());
  sourceMBB->addSuccessor(mbbThrow, BranchProbability::getZero());
  sourceMBB->normalizeSuccProbs();
}

// -----------------------------------------------------------------------------
void ISel::LowerBinary(const Inst *inst, unsigned op)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);

  MVT type = GetVT(binaryInst->GetType());
  SDValue lhs = GetValue(binaryInst->GetLHS());
  SDValue rhs = GetValue(binaryInst->GetRHS());
  SDValue binary = GetDAG().getNode(op, SDL_, type, lhs, rhs);
  Export(inst, binary);
}

// -----------------------------------------------------------------------------
void ISel::LowerShift(const Inst *inst, unsigned op)
{
  auto &DAG = GetDAG();
  auto *binaryInst = static_cast<const BinaryInst *>(inst);
  auto lhs = binaryInst->GetLHS();
  auto rhs = binaryInst->GetRHS();

  MVT type = GetVT(binaryInst->GetType());
  SDValue lhsVal = GetValue(lhs);
  SDValue rhsVal = GetValue(rhs);

  EVT shiftTy = DAG.getTargetLoweringInfo().getShiftAmountTy(
      GetVT(lhs.GetType()),
      DAG.getDataLayout()
  );

  SDValue rhsShift = DAG.getZExtOrTrunc(rhsVal, SDL_, shiftTy);
  SDValue binary = DAG.getNode(op, SDL_, type, lhsVal, rhsShift);
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
    case Type::V64:
    case Type::I128: {
      LowerBinary(inst, iop);
      break;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      LowerBinary(inst, fop);
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerUnary(const UnaryInst *inst, unsigned op)
{
  Type argTy = inst->GetArg().GetType();
  Type retTy = inst->GetType();

  SDValue arg = GetValue(inst->GetArg());
  SDValue unary = GetDAG().getNode(op, SDL_, GetVT(retTy), arg);
  Export(inst, unary);
}

// -----------------------------------------------------------------------------
void ISel::LowerJCC(const JumpCondInst *inst)
{
  llvm::SelectionDAG &DAG = GetDAG();
  auto &TLI = DAG.getTargetLoweringInfo();

  auto *sourceMBB = mbbs_[inst->getParent()];
  auto *trueMBB = mbbs_[inst->GetTrueTarget()];
  auto *falseMBB = mbbs_[inst->GetFalseTarget()];

  ConstRef<Inst> condInst = inst->GetCond();

  if (trueMBB == falseMBB) {
    DAG.setRoot(DAG.getNode(
        ISD::BR,
        SDL_,
        MVT::Other,
        GetExportRoot(),
        DAG.getBasicBlock(trueMBB)
    ));

    sourceMBB->addSuccessor(trueMBB);
  } else {
    SDValue chain = GetExportRoot();
    SDValue cond = GetValue(condInst);

    cond = DAG.getSetCC(
        SDL_,
        TLI.getSetCCResultType(
            DAG.getDataLayout(),
            *DAG.getContext(),
            cond.getValueType()
        ),
        cond,
        DAG.getConstant(0, SDL_, GetVT(condInst.GetType())),
        ISD::CondCode::SETNE
    );

    chain = DAG.getNode(
        ISD::BRCOND,
        SDL_,
        MVT::Other,
        chain,
        cond,
        DAG.getBasicBlock(mbbs_[inst->GetTrueTarget()])
    );

    chain = DAG.getNode(
        ISD::BR,
        SDL_,
        MVT::Other,
        chain,
        DAG.getBasicBlock(mbbs_[inst->GetFalseTarget()])
    );

    DAG.setRoot(chain);

    if (auto *p = inst->GetAnnot<Probability>()) {
      BranchProbability pTrue(p->GetNumerator(), p->GetDenumerator());
      sourceMBB->addSuccessor(trueMBB, pTrue);
      sourceMBB->addSuccessor(falseMBB, pTrue.getCompl());
    } else {
      sourceMBB->addSuccessorWithoutProb(trueMBB);
      sourceMBB->addSuccessorWithoutProb(falseMBB);
    }
  }
  sourceMBB->normalizeSuccProbs();
}

// -----------------------------------------------------------------------------
void ISel::LowerJMP(const JumpInst *inst)
{
  llvm::SelectionDAG &DAG = GetDAG();

  const Block *target = inst->GetTarget();
  auto *sourceMBB = mbbs_[inst->getParent()];
  auto *targetMBB = mbbs_[target];

  DAG.setRoot(DAG.getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      DAG.getBasicBlock(targetMBB)
  ));

  sourceMBB->addSuccessor(targetMBB);
}

// -----------------------------------------------------------------------------
void ISel::LowerSwitch(const SwitchInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &MRI = MF.getRegInfo();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  auto *sourceMBB = mbbs_[inst->getParent()];

  std::vector<llvm::MachineBasicBlock*> branches;
  for (unsigned i = 0, n = inst->getNumSuccessors(); i < n; ++i) {
    auto *mbb = mbbs_[inst->getSuccessor(i)];
    branches.push_back(mbb);
  }

  {
    llvm::DenseSet<llvm::MachineBasicBlock *> added;
    for (auto *mbb : branches) {
      if (added.insert(mbb).second) {
        sourceMBB->addSuccessor(mbb);
      }
    }
  }

  sourceMBB->normalizeSuccProbs();

  // LLVM cannot pattern match if index is a constant (whether lowered or
  // combined into a constant later on), so the index is copied into a vreg
  // which is then passed on the the BR_JT node.
  auto *jti = MF.getOrCreateJumpTableInfo(TLI.getJumpTableEncoding());
  int jumpTableId = jti->createJumpTableIndex(branches);

  MVT idxTy = GetVT(inst->GetIdx().GetType());
  MVT regTy = TLI.getRegisterType(*DAG.getContext(), idxTy);
  auto indexReg = MRI.createVirtualRegister(TLI.getRegClassFor(regTy));

  SDValue chain = DAG.getCopyToReg(
      GetExportRoot(),
      SDL_,
      indexReg,
      DAG.getAnyExtOrTrunc(GetValue(inst->GetIdx()), SDL_, regTy)
  );
  SDValue index = DAG.getAnyExtOrTrunc(
      DAG.getCopyFromReg(chain, SDL_, indexReg, idxTy),
      SDL_,
      idxTy
  );

  SDValue table = DAG.getJumpTable(
      jumpTableId,
      TLI.getPointerTy(DAG.getDataLayout())
  );
  DAG.setRoot(DAG.getNode(
      ISD::BR_JT,
      SDL_,
      MVT::Other,
      index.getValue(1),
      table,
      index
  ));
}

// -----------------------------------------------------------------------------
void ISel::LowerLD(const LoadInst *ld)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type type = ld->GetType();

  SDValue l = dag.getLoad(
      GetVT(type),
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
  llvm::SelectionDAG &DAG = GetDAG();
  ConstRef<Inst> val = st->GetVal();
  DAG.setRoot(DAG.getStore(
      DAG.getRoot(),
      SDL_,
      GetValue(val),
      GetValue(st->GetAddr()),
      llvm::MachinePointerInfo(0u),
      GetAlignment(val.GetType()),
      llvm::MachineMemOperand::MONone,
      llvm::AAMDNodes()
  ));
}

// -----------------------------------------------------------------------------
void ISel::LowerFrame(const FrameInst *inst)
{
  llvm::SelectionDAG &DAG = GetDAG();
  auto ptrVT = DAG.getTargetLoweringInfo().getPointerTy(DAG.getDataLayout());

  if (auto It = stackIndices_.find(inst->GetObject()); It != stackIndices_.end()) {
    SDValue base = DAG.getFrameIndex(It->second, ptrVT);
    if (auto offset = inst->GetOffset()) {
      Export(inst, DAG.getNode(
          ISD::ADD,
          SDL_,
          ptrVT,
          base,
          DAG.getConstant(offset, SDL_, ptrVT)
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
  auto &DAG = GetDAG();
  auto &TLI = DAG.getTargetLoweringInfo();

  MVT type = GetVT(cmpInst->GetType());
  SDValue lhs = GetValue(cmpInst->GetLHS());
  SDValue rhs = GetValue(cmpInst->GetRHS());
  ISD::CondCode cc = GetCond(cmpInst->GetCC());
  llvm::EVT flagTy = TLI.getSetCCResultType(
      DAG.getDataLayout(),
      *DAG.getContext(),
      lhs.getValueType()
  );
  SDValue flag = DAG.getSetCC(SDL_, flagTy, lhs, rhs, cc);
  Export(cmpInst, DAG.getZExtOrTrunc(flag, SDL_, type));
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

  ConstRef<Value> val = GetMoveArg(inst);
  switch (val->GetKind()) {
    case Value::Kind::INST: {
      ConstRef<Inst> arg = ::cast_or_null<Inst>(val);
      Type argType = arg.GetType();
      if (CompatibleType(argType, retType)) {
        Export(inst, GetValue(arg));
      } else if (GetSize(argType) == GetSize(retType)) {
        Export(inst, GetDAG().getBitcast(GetVT(retType), GetValue(arg)));
      } else {
        Error(inst, "unsupported mov");
      }
      return;
    }
    case Value::Kind::CONST: {
      const Constant &constVal = *::cast_or_null<Constant>(val);
      switch (constVal.GetKind()) {
        case Constant::Kind::REG: {
          auto &constReg = static_cast<const ConstantReg &>(constVal);
          Export(inst, LoadReg(constReg.GetValue()));
          return;
        }
        case Constant::Kind::INT:
        case Constant::Kind::FLOAT: {
          return;
        }
      }
      llvm_unreachable("invalid constant kind");
    }
    case Value::Kind::GLOBAL:
    case Value::Kind::EXPR: {
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void ISel::LowerSExt(const SExtInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();

  Type argTy = inst->GetArg().GetType();
  Type retTy = inst->GetType();
  MVT retMVT = GetVT(retTy);
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

  Type argTy = inst->GetArg().GetType();
  Type retTy = inst->GetType();
  MVT retMVT = GetVT(retTy);
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
  Type argTy = inst->GetArg().GetType();
  Type retTy = inst->GetType();
  MVT retMVT = GetVT(retTy);
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

  Type argTy = inst->GetArg().GetType();
  Type retTy = inst->GetType();

  if (!IsFloatType(argTy) || !IsFloatType(retTy)) {
    Error(inst, "argument/return not a float");
  }
  if (GetSize(argTy) >= GetSize(retTy)) {
    Error(inst, "Cannot shrink argument");
  }

  SDValue arg = GetValue(inst->GetArg());
  SDValue fext = dag.getNode(ISD::FP_EXTEND, SDL_, GetVT(retTy), arg);
  Export(inst, fext);
}

// -----------------------------------------------------------------------------
void ISel::LowerTrunc(const TruncInst *inst)
{
  auto &DAG = GetDAG();
  auto &TLI = DAG.getTargetLoweringInfo();
  auto ptrVT = TLI.getPointerTy(DAG.getDataLayout());

  Type argTy = inst->GetArg().GetType();
  Type retTy = inst->GetType();

  MVT retMVT = GetVT(retTy);
  SDValue arg = GetValue(inst->GetArg());

  unsigned opcode;
  if (IsFloatType(retTy)) {
    if (IsIntegerType(argTy)) {
      Error(inst, "Cannot truncate int -> float");
    } else {
      if (argTy == retTy) {
        Export(inst, DAG.getNode(ISD::FTRUNC, SDL_, retMVT, arg));
      } else {
        Export(inst, DAG.getNode(
            ISD::FP_ROUND,
            SDL_,
            retMVT,
            arg,
            DAG.getTargetConstant(0, SDL_, ptrVT)
        ));
      }
    }
  } else {
    if (IsIntegerType(argTy)) {
      Export(inst, DAG.getNode(ISD::TRUNCATE, SDL_, retMVT, arg));
    } else {
      Export(inst, DAG.getNode(ISD::FP_TO_SINT, SDL_, retMVT, arg));
    }
  }
}

// -----------------------------------------------------------------------------
void ISel::LowerAlloca(const AllocaInst *inst)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &TLI = *MF.getSubtarget().getTargetLowering();

  // Mark the frame as one containing variable-sized objects.
  MF.getFrameInfo().setHasVarSizedObjects(true);

  unsigned Align = inst->GetAlign();
  SDValue Size = GetValue(inst->GetCount());
  MVT VT = GetVT(inst->GetType());

  SDValue node = DAG.getNode(
      ISD::DYNAMIC_STACKALLOC,
      SDL_,
      DAG.getVTList(VT, MVT::Other),
      {
          DAG.getRoot(),
          Size,
          DAG.getConstant(Align, SDL_, TLI.getPointerTy(DAG.getDataLayout()))
      }
  );

  DAG.setRoot(node.getValue(1));
  Export(inst, node);
}

// -----------------------------------------------------------------------------
void ISel::LowerSelect(const SelectInst *select)
{
  auto &DAG = GetDAG();
  auto &TLI = DAG.getTargetLoweringInfo();

  ConstRef<Inst> condInst = select->GetCond();
  SDValue cond = GetValue(condInst);
  llvm::EVT condVT = GetVT(condInst.GetType());

  llvm::EVT flagTy = TLI.getSetCCResultType(
      DAG.getDataLayout(),
      *DAG.getContext(),
      condVT
  );

  SDValue node = DAG.getNode(
      ISD::SELECT,
      SDL_,
      GetVT(select->GetType()),
      DAG.getSetCC(
          SDL_,
          flagTy,
          cond,
          DAG.getConstant(0, SDL_, condVT),
          ISD::CondCode::SETNE
      ),
      GetValue(select->GetTrue()),
      GetValue(select->GetFalse())
  );
  Export(select, node);
}

// -----------------------------------------------------------------------------
void ISel::LowerUndef(const UndefInst *inst)
{
  Export(inst, GetDAG().getUNDEF(GetVT(inst->GetType())));
}

// -----------------------------------------------------------------------------
void ISel::LowerALUO(const OverflowInst *inst, unsigned op)
{
  llvm::SelectionDAG &dag = GetDAG();

  MVT retType = GetVT(inst->GetType());
  MVT type = GetVT(inst->GetLHS().GetType());
  SDValue lhs = GetValue(inst->GetLHS());
  SDValue rhs = GetValue(inst->GetRHS());

  SDVTList types = dag.getVTList(type, MVT::i1);
  SDValue node = dag.getNode(op, SDL_, types, lhs, rhs);
  SDValue flag = dag.getZExtOrTrunc(node.getValue(1), SDL_, retType);

  Export(inst, flag);
}

// -----------------------------------------------------------------------------
llvm::SDValue ISel::LowerCallArguments(
    SDValue chain,
    const CallSite *call,
    CallLowering &ci,
    llvm::SmallVectorImpl<std::pair<unsigned, SDValue>> &regs)
{
  auto &DAG = GetDAG();
  auto &MF = DAG.getMachineFunction();
  auto &TLI = *MF.getSubtarget().getTargetLowering();
  auto ptrVT = TLI.getPointerTy(DAG.getDataLayout());

  llvm::SmallVector<SDValue, 8> memOps;
  SDValue stackPtr;
  for (auto it = ci.arg_begin(); it != ci.arg_end(); ++it) {
    ConstRef<Inst> arg = call->arg(it->Index);
    SDValue argument = GetValue(arg);
    const MVT argVT = GetVT(arg.GetType());
    for (unsigned i = 0, n = it->Parts.size(); i < n; ++i) {
      auto &part = it->Parts[i];

      SDValue value;
      if (n == 1) {
        if (argVT != part.VT) {
          if (argVT.isFloatingPoint()) {
            value = DAG.getAnyExtOrTrunc(
                DAG.getBitcast(
                    llvm::MVT::getIntegerVT(argVT.getSizeInBits()),
                    argument
                ),
                SDL_,
                part.VT
            );
          } else {
            value = DAG.getAnyExtOrTrunc(argument, SDL_, part.VT);
          }
        } else {
          value = argument;
        }
      } else {
        value = DAG.getNode(
            ISD::EXTRACT_ELEMENT,
            SDL_,
            part.VT,
            argument,
            DAG.getConstant(i, SDL_, part.VT)
        );
      }

      switch (part.K) {
        case CallLowering::ArgPart::Kind::REG: {
          regs.emplace_back(part.Reg, value);
          break;
        }
        case CallLowering::ArgPart::Kind::STK: {
          if (!stackPtr.getNode()) {
            stackPtr = DAG.getCopyFromReg(
                chain,
                SDL_,
                GetStackRegister(),
                ptrVT
            );
          }

          SDValue memOff = DAG.getNode(
              ISD::ADD,
              SDL_,
              ptrVT,
              stackPtr,
              DAG.getIntPtrConstant(part.Offset, SDL_)
          );

          memOps.push_back(DAG.getStore(
              chain,
              SDL_,
              value,
              memOff,
              llvm::MachinePointerInfo::getStack(MF, part.Offset)
          ));

          break;
        }
      }
    }
  }

  if (memOps.empty()) {
    return chain;
  }
  return DAG.getNode(ISD::TokenFactor, SDL_, MVT::Other, memOps);
}

std::pair<llvm::SDValue, llvm::SDValue>
ISel::LowerReturns(
    SDValue chain,
    SDValue inFlag,
    const CallSite *call,
    llvm::SmallVectorImpl<CallLowering::RetLoc> &returns,
    llvm::SmallVectorImpl<SDValue> &regs,
    llvm::SmallVectorImpl<std::pair<ConstRef<Inst>, SDValue>> &values)
{
  auto &DAG = GetDAG();
  for (const auto &retLoc : returns) {
    llvm::SmallVector<SDValue, 2> parts;
    for (auto &part : retLoc.Parts) {
      SDValue copy = DAG.getCopyFromReg(
          DAG.getEntryNode(),
          SDL_,
          part.Reg,
          part.VT,
          inFlag
      );
      chain = copy.getValue(1);
      if (inFlag) {
        inFlag = copy.getValue(2);
      }
      parts.push_back(copy.getValue(0));
      regs.push_back(DAG.getRegister(part.Reg, part.VT));
    }

    SDValue ret;
    MVT vt = GetVT(call->type(retLoc.Index));
    switch (parts.size()) {
      default: case 0: {
        llvm_unreachable("invalid partition");
      }
      case 1: {
        ret = parts[0];
        if (vt != ret.getSimpleValueType()) {
          ret = DAG.getAnyExtOrTrunc(ret, SDL_, vt);
        }
        break;
      }
      case 2: {
        ret = DAG.getNode(
            ISD::BUILD_PAIR,
            SDL_,
            vt,
            parts[0],
            parts[1]
        );
        break;
      }
    }

    values.emplace_back(call->GetSubValue(retLoc.Index), ret);
  }
  return { chain, inFlag };
}
