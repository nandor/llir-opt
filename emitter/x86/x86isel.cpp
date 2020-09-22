// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.


#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Mangler.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/data.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "emitter/x86/x86call.h"
#include "emitter/x86/x86isel.h"

namespace ISD = llvm::ISD;
namespace X86 = llvm::X86;
namespace X86ISD = llvm::X86ISD;
using MVT = llvm::MVT;
using EVT = llvm::EVT;
using GlobalValue = llvm::GlobalValue;
using SDNodeFlags = llvm::SDNodeFlags;
using SDNode = llvm::SDNode;
using SDValue = llvm::SDValue;
using SDVTList = llvm::SDVTList;
using SelectionDAG = llvm::SelectionDAG;
using X86RegisterInfo = llvm::X86RegisterInfo;
using GlobalAddressSDNode = llvm::GlobalAddressSDNode;
using ConstantSDNode = llvm::ConstantSDNode;
using BranchProbability = llvm::BranchProbability;

// -----------------------------------------------------------------------------
BranchProbability kLikely = BranchProbability::getBranchProbability(99, 100);
BranchProbability kUnlikely = BranchProbability::getBranchProbability(1, 100);

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
    llvm::CodeGenOpt::Level OL,
    bool shared)
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , X86DAGMatcher(*TM, OL, STI)
  , ModulePass(ID)
  , TM_(TM)
  , TRI_(TRI)
  , LibInfo_(LibInfo)
  , prog_(prog)
  , trampoline_(nullptr)
  , shared_(shared)
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

  if (auto *movInst = ::dyn_cast_or_null<const MovInst>(inst)) {
    auto *val = movInst->GetArg();
    switch (val->GetKind()) {
      case Value::Kind::INST:
        break;
      case Value::Kind::CONST: {
        switch (static_cast<Constant *>(val)->GetKind()) {
          case Constant::Kind::REG:
            break;
          case Constant::Kind::INT:
          case Constant::Kind::FLOAT:
            return false;
        }
        break;
      }
      case Value::Kind::GLOBAL:
      case Value::Kind::EXPR:
        return false;
    }
  }
  const Block *parent = inst->getParent();
  for (const User *user : inst->users()) {
    auto *value = static_cast<const Inst *>(user);
    if (value->getParent() != parent || value->Is(Inst::Kind::PHI)) {
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
  i8PtrTy_ = llvm::Type::getInt1PtrTy (Ctx);
  funcTy_ = llvm::FunctionType::get(voidTy_, {});

  // Create function definitions for all functions.
  for (const Func &func : *prog_) {
    // Determine the LLVM linkage type.
    GlobalValue::LinkageTypes linkage;
    if (func.IsExported() || !func.IsHidden()) {
      linkage = GlobalValue::ExternalLinkage;
    } else {
      linkage = GlobalValue::InternalLinkage;
    }

    // Add a dummy function to the module.
    auto *F = llvm::Function::Create(funcTy_, linkage, 0, func.getName(), M);

    // Set a dummy calling conv to emulate the set
    // of registers preserved by the callee.
    llvm::CallingConv::ID cc;
    switch (func.GetCallingConv()) {
      case CallingConv::C:          cc = llvm::CallingConv::C;               break;
      case CallingConv::CAML:       cc = llvm::CallingConv::LLIR_CAML;       break;
      case CallingConv::CAML_RAISE: cc = llvm::CallingConv::LLIR_CAML_RAISE; break;
      case CallingConv::SETJMP:     cc = llvm::CallingConv::LLIR_SETJMP;     break;
      case CallingConv::CAML_ALLOC: llvm_unreachable("cannot define caml_alloc");
      case CallingConv::CAML_GC:    llvm_unreachable("cannot define caml_");
    }
    F->setCallingConv(cc);
    llvm::BasicBlock* block = llvm::BasicBlock::Create(F->getContext(), "entry", F);
    llvm::IRBuilder<> builder(block);
    builder.CreateRetVoid();
  }

  // Create function declarations for externals.
  for (const Global &ext : prog_->externs()) {
    M->getOrInsertFunction(ext.getName(), funcTy_);
  }

  // Add symbols for data values.
  for (const auto &data : prog_->data()) {
    LowerData(&data);
  }

  // Generate code for functions.
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  for (const Func &func : *prog_) {
    // Save a pointer to the current function.
    liveOnExit_.clear();
    func_ = &func;
    conv_ = std::make_unique<X86Call>(&func);
    lva_ = nullptr;
    frameIndex_ = 0;
    stackIndices_.clear();

    // Create a new dummy empty Function.
    // The IR function simply returns void since it cannot be empty.
    F = M->getFunction(func.GetName().data());

    // Create a MachineFunction, attached to the dummy one.
    auto ORE = std::make_unique<llvm::OptimizationRemarkEmitter>(F);
    MF = &MMI.getOrCreateMachineFunction(*F);
    funcs_[&func] = MF;
    MF->setAlignment(llvm::Align(func.GetAlignment()));
    FuncInfo_ = MF->getInfo<llvm::X86MachineFunctionInfo>();

    // Initialise the dag with info for this function.
    llvm::FunctionLoweringInfo FLI;
    CurDAG->init(*MF, *ORE, this, LibInfo_, nullptr, nullptr, nullptr);
    CurDAG->setFunctionLoweringInfo(&FLI);

    // Traverse nodes, entry first.
    llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);

    // Flag indicating if the function has VASTART.
    bool hasVAStart = false;

    // Create a MBB for all LLIR blocks, isolating the entry block.
    const Block *entry = nullptr;
    llvm::MachineBasicBlock *entryMBB = nullptr;
    auto *RegInfo = &MF->getRegInfo();

    for (const Block &block : func) {
      // Create a skeleton basic block, with a jump to itself.
      llvm::BasicBlock *BB = llvm::BasicBlock::Create(
          M->getContext(),
          block.GetName().data(),
          F,
          nullptr
      );
      llvm::BranchInst::Create(BB, BB);

      // Create the basic block to be filled in by the instruction selector.
      llvm::MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(BB);
      MBB->setHasAddressTaken();
      blocks_[&block] = MBB;
      MF->push_back(MBB);
    }

    for (const Block *block : blockOrder) {
      // First block in reverse post-order is the entry block.
      llvm::MachineBasicBlock *MBB = FLI.MBB = blocks_[block];
      entry = entry ? entry : block;
      entryMBB = entryMBB ? entryMBB : MBB;

      // Allocate registers for exported values and create PHI
      // instructions for all PHI nodes in the basic block.
      for (const auto &inst : *block) {
        if (inst.Is(Inst::Kind::PHI)) {
          if (inst.use_empty()) {
            continue;
          }
          // Create a machine PHI instruction for all PHIs. The order of
          // machine PHIs should match the order of PHIs in the block.
          auto &phi = static_cast<const PhiInst &>(inst);
          auto reg = AssignVReg(&phi);
          BuildMI(MBB, DL_, TII->get(llvm::TargetOpcode::PHI), reg);
        } else if (inst.Is(Inst::Kind::ARG)) {
          // If the arg is used outside of entry, export it.
          auto &arg = static_cast<const ArgInst &>(inst);
          bool usedOutOfEntry = false;
          for (const User *user : inst.users()) {
            auto *value = static_cast<const Inst *>(user);
            if (usedOutOfEntry || value->getParent() != entry) {
              AssignVReg(&arg);
              break;
            }
          }
        } else if (IsExported(&inst)) {
          // If the value is used outside of the defining block, export it.
          AssignVReg(&inst);
        }

        if (inst.Is(Inst::Kind::VASTART)) {
          hasVAStart = true;
        }
      }
    }

    // Lower individual blocks.
    for (const Block *block : blockOrder) {
      MBB_ = blocks_[block];

      {
        // If this is the entry block, lower all arguments.
        if (block == entry) {
          if (hasVAStart) {
            LowerVASetup(func, *conv_);
          }
          for (auto &argLoc : conv_->args()) {
            LowerArg(func, argLoc);
          }

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

        // Set up the SelectionDAG for the block.
        for (const auto &inst : *block) {
          Lower(&inst);
        }
      }

      // Ensure all values were exported.
      assert(pendingExports_.empty() && "not all values were exported");

      // Lower the block.
      insert_ = MBB_->end();
      CodeGenAndEmitDAG();
      BundleAnnotations(block, MBB_);

      // Clear values, except exported ones.
      values_.clear();
    }

    // If the entry block has a predecessor, insert a dummy entry.
    if (entryMBB->pred_size() != 0) {
      MBB_ = MF->CreateMachineBasicBlock();
      CurDAG->setRoot(CurDAG->getNode(
          ISD::BR,
          SDL_,
          MVT::Other,
          CurDAG->getRoot(),
          CurDAG->getBasicBlock(entryMBB)
      ));

      insert_ = MBB_->end();
      CodeGenAndEmitDAG();

      MF->push_front(MBB_);
      MBB_->addSuccessor(entryMBB);
      entryMBB = MBB_;
    }

    // Emit copies from args into vregs at the entry.
    const auto &TRI = *MF->getSubtarget().getRegisterInfo();
    RegInfo->EmitLiveInCopies(entryMBB, TRI, *TII);

    TLI->finalizeLowering(*MF);

    MF->verify(nullptr, "LLIR-to-X86 ISel");

    MBB_ = nullptr;
    MF = nullptr;
  }

  // Finalize lowering of references.
  for (const auto &data : prog_->data()) {
    LowerRefs(&data);
  }

  return true;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerData(const Data *data)
{
  for (const Object &object : *data) {
    for (const Atom &atom : object) {
      auto *GV = new llvm::GlobalVariable(
          *M,
          i8PtrTy_,
          false,
          llvm::GlobalValue::ExternalLinkage,
          nullptr,
          atom.GetName().data()
      );
      GV->setDSOLocal(true);
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerRefs(const Data *data)
{
  for (const Object &object : *data) {
    for (const Atom &atom : object) {
      for (const Item &item : atom) {
        if (item.GetKind() != Item::Kind::EXPR) {
          continue;
        }

        auto *expr = item.GetExpr();
        switch (expr->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto *offsetExpr = static_cast<SymbolOffsetExpr *>(expr);
            if (auto *block = ::dyn_cast_or_null<Block>(offsetExpr->GetSymbol())) {
              auto *MBB = blocks_[block];
              auto *BB = const_cast<llvm::BasicBlock *>(MBB->getBasicBlock());

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

// -----------------------------------------------------------------------------
void X86ISel::LowerReturn(const ReturnInst *retInst)
{
  llvm::SmallVector<SDValue, 6> returns;
  returns.push_back(SDValue());
  returns.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));

  for (auto &reg : liveOnExit_) {
    returns.push_back(CurDAG->getRegister(reg, MVT::i64));
  }

  SDValue flag;
  SDValue chain = GetExportRoot();
  if (auto *retVal = retInst->GetValue()) {
    Type retType = retVal->GetType(0);
    unsigned retReg;
    switch (retType) {
      case Type::I8:  retReg = X86::AL;   break;
      case Type::I16: retReg = X86::AX;   break;
      case Type::I64: retReg = X86::RAX;  break;
      case Type::I32: retReg = X86::EAX;  break;
      case Type::F32: retReg = X86::XMM0; break;
      case Type::F64: retReg = X86::XMM0; break;
      default: Error(retInst, "Invalid return type");
    }

    auto it = liveOnExit_.find(retReg);
    if (it != liveOnExit_.end()) {
      Error(retInst, "Set register is live on exit");
    }

    SDValue arg = GetValue(retVal);
    chain = CurDAG->getCopyToReg(chain, SDL_, retReg, arg, flag);
    returns.push_back(CurDAG->getRegister(retReg, GetType(retType)));
    flag = chain.getValue(1);
  }

  returns[0] = chain;
  if (flag.getNode()) {
    returns.push_back(flag);
  }

  CurDAG->setRoot(CurDAG->getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, returns));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCall(const CallInst *inst)
{
  LowerCallSite(CurDAG->getRoot(), inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTailCall(const TailCallInst *inst)
{
  LowerCallSite(CurDAG->getRoot(), inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerInvoke(const InvokeInst *inst)
{
  auto &MMI = MF->getMMI();
  auto *bCont = inst->GetCont();
  auto *bThrow = inst->GetThrow();
  auto *mbbCont = blocks_[bCont];
  auto *mbbThrow = blocks_[bThrow];

  // Mark the landing pad as such.
  mbbThrow->setIsEHPad();

  // Lower the invoke call: export here since the call might not return.
  LowerCallSite(GetExportRoot(), inst);

  // Add a jump to the continuation block: export the invoke result.
  CurDAG->setRoot(CurDAG->getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      CurDAG->getBasicBlock(mbbCont)
  ));

  // Mark successors.
  auto *sourceMBB = blocks_[inst->getParent()];
  sourceMBB->addSuccessor(mbbCont, BranchProbability::getOne());
  sourceMBB->addSuccessor(mbbThrow, BranchProbability::getZero());
  sourceMBB->normalizeSuccProbs();
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTailInvoke(const TailInvokeInst *inst)
{
  auto &MMI = MF->getMMI();
  auto *bThrow = inst->GetThrow();
  auto *mbbThrow = blocks_[bThrow];

  // Mark the landing pad as such.
  mbbThrow->setIsEHPad();

  // Lower the invoke call.
  LowerCallSite(GetExportRoot(), inst);

  // Mark successors.
  auto *sourceMBB = blocks_[inst->getParent()];
  sourceMBB->addSuccessor(mbbThrow);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCmpXchg(const CmpXchgInst *inst)
{
  auto *mmo = MF->getMachineMemOperand(
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

  unsigned reg;
  unsigned size;
  MVT type;
  switch (inst->GetType()) {
  case Type::I8:  reg = X86::AL;  size = 1; type = MVT::i8; break;
  case Type::I16: reg = X86::AX;  size = 2; type = MVT::i16; break;
  case Type::I32: reg = X86::EAX; size = 4; type = MVT::i32; break;
  case Type::I64: reg = X86::RAX; size = 8; type = MVT::i64; break;
  case Type::I128:
    Error(inst, "invalid type");
  case Type::F32: case Type::F64: case Type::F80:
    Error(inst, "invalid type");
  }

  SDValue writeReg = CurDAG->getCopyToReg(
      CurDAG->getRoot(),
      SDL_,
      reg,
      GetValue(inst->GetRef()),
      SDValue()
  );
  SDValue ops[] = {
      writeReg.getValue(0),
      GetValue(inst->GetAddr()),
      GetValue(inst->GetVal()),
      CurDAG->getTargetConstant(size, SDL_, MVT::i8),
      writeReg.getValue(1)
   };
  SDValue cmpXchg = CurDAG->getMemIntrinsicNode(
      X86ISD::LCMPXCHG_DAG,
      SDL_,
      CurDAG->getVTList(MVT::Other, MVT::Glue),
      ops,
      type,
      mmo
  );
  SDValue readReg = CurDAG->getCopyFromReg(
      cmpXchg.getValue(0),
      SDL_,
      reg,
      type,
      cmpXchg.getValue(1)
  );
  CurDAG->setRoot(readReg.getValue(1));
  Export(inst, readReg.getValue(0));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSet(const SetInst *inst)
{
  auto value = GetValue(inst->GetValue());

  auto setReg = [&value, this](auto reg) {
    CurDAG->setRoot(CurDAG->getCopyToReg(CurDAG->getRoot(), SDL_, reg, value));
    liveOnExit_.insert(reg);
  };

  switch (inst->GetReg()->GetValue()) {
    // X86 architectural register.
    case ConstantReg::Kind::RAX: setReg(X86::RAX); break;
    case ConstantReg::Kind::RBX: setReg(X86::RBX); break;
    case ConstantReg::Kind::RCX: setReg(X86::RCX); break;
    case ConstantReg::Kind::RDX: setReg(X86::RDX); break;
    case ConstantReg::Kind::RSI: setReg(X86::RSI); break;
    case ConstantReg::Kind::RDI: setReg(X86::RDI); break;
    case ConstantReg::Kind::RSP: setReg(X86::RSP); break;
    case ConstantReg::Kind::RBP: setReg(X86::RBP); break;
    case ConstantReg::Kind::R8:  setReg(X86::R8);  break;
    case ConstantReg::Kind::R9:  setReg(X86::R9);  break;
    case ConstantReg::Kind::R10: setReg(X86::R10); break;
    case ConstantReg::Kind::R11: setReg(X86::R11); break;
    case ConstantReg::Kind::R12: setReg(X86::R12); break;
    case ConstantReg::Kind::R13: setReg(X86::R13); break;
    case ConstantReg::Kind::R14: setReg(X86::R14); break;
    case ConstantReg::Kind::R15: setReg(X86::R15); break;
    case ConstantReg::Kind::FS:  setReg(X86::FS);  break;
    // Program counter.
    case ConstantReg::Kind::PC: {
      Error(inst, "Cannot rewrite program counter");
    }
    // Frame address.
    case ConstantReg::Kind::FRAME_ADDR: {
      Error(inst, "Cannot rewrite frame address");
    }
    // Return address.
    case ConstantReg::Kind::RET_ADDR: {
      Error(inst, "Cannot rewrite return address");
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerVAStart(const VAStartInst *inst)
{
  if (!inst->getParent()->getParent()->IsVarArg()) {
    Error(inst, "vastart in a non-vararg function");
  }

  CurDAG->setRoot(CurDAG->getNode(
      ISD::VASTART,
      SDL_,
      MVT::Other,
      CurDAG->getRoot(),
      GetValue(inst->GetVAList()),
      CurDAG->getSrcValue(nullptr)
  ));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerRDTSC(const RdtscInst *inst)
{
  switch (inst->GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32: {
      llvm_unreachable("not implemented");
    }
    case Type::I64: {
      SDVTList Tys = CurDAG->getVTList(MVT::Other, MVT::Glue);
      SDValue Read = SDValue(CurDAG->getMachineNode(
          X86::RDTSC,
          SDL_,
          Tys,
          CurDAG->getRoot()
      ), 0);

      SDValue LO = CurDAG->getCopyFromReg(
          Read,
          SDL_,
          X86::RAX,
          MVT::i64,
          Read.getValue(1)
      );
      SDValue HI = CurDAG->getCopyFromReg(
          LO.getValue(1),
          SDL_,
          X86::RDX,
          MVT::i64,
          LO.getValue(2)
      );

      SDValue TSC = CurDAG->getNode(
          ISD::OR,
          SDL_,
          MVT::i64,
          LO,
          CurDAG->getNode(
              ISD::SHL,
              SDL_,
              MVT::i64, HI,
              CurDAG->getConstant(32, SDL_, MVT::i8)
          )
      );
      Export(inst, TSC);
      CurDAG->setRoot(HI.getValue(1));
      return;
    }
    case Type::I128: {
      llvm_unreachable("not implemented");
    }
    case Type::F32: case Type::F64: case Type::F80: {
      llvm_unreachable("not implemented");
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFNStCw(const FNStCwInst *inst)
{
  auto *mmo = MF->getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOStore,
      2,
      llvm::Align(1),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue addr = GetValue(inst->GetAddr());
  SDValue Ops[] = { CurDAG->getRoot(), addr };
  CurDAG->setRoot(
      CurDAG->getMemIntrinsicNode(
          X86ISD::FNSTCW16m,
          SDL_,
          CurDAG->getVTList(MVT::Other),
          Ops,
          MVT::i16,
          mmo
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFLdCw(const FLdCwInst *inst)
{
  auto *mmo = MF->getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad,
      2,
      llvm::Align(1),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue addr = GetValue(inst->GetAddr());
  SDValue Ops[] = { CurDAG->getRoot(), addr };
  CurDAG->setRoot(
      CurDAG->getMemIntrinsicNode(
          X86ISD::FLDCW16m,
          SDL_,
          CurDAG->getVTList(MVT::Other),
          Ops,
          MVT::i16,
          mmo
      )
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArg(const Func &func, X86Call::Loc &argLoc)
{
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());

  const llvm::TargetRegisterClass *regClass;
  MVT regType;
  unsigned size;
  switch (argLoc.ArgType) {
    case Type::I8:{
      regType = MVT::i8;
      regClass = &X86::GR8RegClass;
      size = 1;
      break;
    }
    case Type::I16:{
      regType = MVT::i16;
      regClass = &X86::GR16RegClass;
      size = 2;
      break;
    }
    case Type::I32: {
      regType = MVT::i32;
      regClass = &X86::GR32RegClass;
      size = 4;
      break;
    }
    case Type::I64: {
      regType = MVT::i64;
      regClass = &X86::GR64RegClass;
      size = 8;
      break;
    }
    case Type::I128: {
      Error(&func, "Invalid argument to call.");
    }
    case Type::F32: {
      regType = MVT::f32;
      regClass = &X86::FR32RegClass;
      size = 4;
      break;
    }
    case Type::F64: {
      regType = MVT::f64;
      regClass = &X86::FR64RegClass;
      size = 8;
      break;
    }
    case Type::F80: {
      regType = MVT::f80;
      regClass = &X86::RFP80RegClass;
      size = 10;
      break;
    }
  }

  SDValue arg;
  switch (argLoc.Kind) {
    case X86Call::Loc::Kind::REG: {
      unsigned Reg = MF->addLiveIn(argLoc.Reg, regClass);
      arg = CurDAG->getCopyFromReg(CurDAG->getEntryNode(), SDL_, Reg, regType);
      break;
    }
    case X86Call::Loc::Kind::STK: {
      llvm::MachineFrameInfo &MFI = MF->getFrameInfo();
      int index = MFI.CreateFixedObject(size, argLoc.Idx, true);

      args_[argLoc.Index] = index;

      arg = CurDAG->getLoad(
          regType,
          SDL_,
          CurDAG->getEntryNode(),
          CurDAG->getFrameIndex(index, ptrTy),
          llvm::MachinePointerInfo::getFixedStack(
              CurDAG->getMachineFunction(),
              index
          )
      );
      break;
    }
  }

  for (const auto &block : func) {
    for (const auto &inst : block) {
      if (!inst.Is(Inst::Kind::ARG)) {
        continue;
      }
      auto &argInst = static_cast<const ArgInst &>(inst);
      if (argInst.GetIdx() == argLoc.Index) {
        Export(&argInst, arg);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerVASetup(const Func &func, X86Call &ci)
{
  llvm::MachineFrameInfo &MFI = MF->getFrameInfo();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());
  SDValue chain = CurDAG->getRoot();

  // Get the size of the stack, plus alignment to store the return
  // address for tail calls for the fast calling convention.
  unsigned stackSize = ci.GetFrameSize();
  switch (func.GetCallingConv()) {
    case CallingConv::C: {
      break;
    }
    case CallingConv::SETJMP:
    case CallingConv::CAML:
    case CallingConv::CAML_ALLOC:
    case CallingConv::CAML_GC:
    case CallingConv::CAML_RAISE: {
      Error(&func, "vararg call not supported");
    }
  }

  int index = MFI.CreateFixedObject(1, stackSize, false);
  FuncInfo_->setVarArgsFrameIndex(index);

  // Copy all unused regs to be pushed on the stack into vregs.
  llvm::SmallVector<SDValue, 6> liveGPRs;
  llvm::SmallVector<SDValue, 8> liveXMMs;
  SDValue alReg;

  for (unsigned reg : ci.GetUnusedGPRs()) {
    unsigned vreg = MF->addLiveIn(reg, &X86::GR64RegClass);
    liveGPRs.push_back(CurDAG->getCopyFromReg(chain, SDL_, vreg, MVT::i64));
  }

  for (unsigned reg : ci.GetUnusedXMMs()) {
    if (!alReg) {
      unsigned vreg = MF->addLiveIn(X86::AL, &X86::GR8RegClass);
      alReg = CurDAG->getCopyFromReg(chain, SDL_, vreg, MVT::i8);
    }
    unsigned vreg = MF->addLiveIn(reg, &X86::VR128RegClass);
    liveXMMs.push_back(CurDAG->getCopyFromReg(chain, SDL_, vreg, MVT::v4f32));
  }

  // Save the indices to be stored in __va_list_tag
  unsigned numGPRs = ci.GetUnusedGPRs().size() + ci.GetUsedGPRs().size();
  unsigned numXMMs = ci.GetUnusedXMMs().size() + ci.GetUsedXMMs().size();
  FuncInfo_->setVarArgsGPOffset(ci.GetUsedGPRs().size() * 8);
  FuncInfo_->setVarArgsFPOffset(numGPRs * 8 + ci.GetUsedXMMs().size() * 16);
  FuncInfo_->setRegSaveFrameIndex(MFI.CreateStackObject(
      numGPRs * 8 + numXMMs * 16,
      llvm::Align(16),
      false
  ));

  llvm::SmallVector<SDValue, 8> storeOps;
  SDValue frameIdx = CurDAG->getFrameIndex(
      FuncInfo_->getRegSaveFrameIndex(),
      ptrTy
  );

  // Store the unused GPR registers on the stack.
  unsigned gpOffset = FuncInfo_->getVarArgsGPOffset();
  for (SDValue val : liveGPRs) {
    SDValue valIdx = CurDAG->getNode(
        ISD::ADD,
        SDL_,
        ptrTy,
        frameIdx,
        CurDAG->getIntPtrConstant(gpOffset, SDL_)
    );
    storeOps.push_back(CurDAG->getStore(
        val.getValue(1),
        SDL_,
        val,
        valIdx,
        llvm::MachinePointerInfo::getFixedStack(
            CurDAG->getMachineFunction(),
            FuncInfo_->getRegSaveFrameIndex(),
            gpOffset
        )
    ));
    gpOffset += 8;
  }

  // Store the unused XMMs on the stack.
  if (!liveXMMs.empty()) {
    llvm::SmallVector<SDValue, 12> ops;
    ops.push_back(chain);
    ops.push_back(alReg);
    ops.push_back(CurDAG->getIntPtrConstant(
        FuncInfo_->getRegSaveFrameIndex(), SDL_)
    );
    ops.push_back(CurDAG->getIntPtrConstant(
        FuncInfo_->getVarArgsFPOffset(), SDL_)
    );
    ops.insert(ops.end(), liveXMMs.begin(), liveXMMs.end());
    storeOps.push_back(CurDAG->getNode(
        X86ISD::VASTART_SAVE_XMM_REGS,
        SDL_,
        MVT::Other,
        ops
    ));
  }

  if (!storeOps.empty()) {
    chain = CurDAG->getNode(ISD::TokenFactor, SDL_, MVT::Other, storeOps);
  }

  CurDAG->setRoot(chain);
}

// -----------------------------------------------------------------------------
SDValue X86ISel::LoadReg(ConstantReg::Kind reg)
{
  auto copyFrom = [this](auto reg) {
    unsigned vreg = MF->addLiveIn(reg, &X86::GR64RegClass);
    auto copy = CurDAG->getCopyFromReg(CurDAG->getRoot(), SDL_, vreg, MVT::i64);
    return copy.getValue(0);
  };

  switch (reg) {
    // X86 architectural registers.
    case ConstantReg::Kind::RAX: return copyFrom(X86::RAX);
    case ConstantReg::Kind::RBX: return copyFrom(X86::RBX);
    case ConstantReg::Kind::RCX: return copyFrom(X86::RCX);
    case ConstantReg::Kind::RDX: return copyFrom(X86::RDX);
    case ConstantReg::Kind::RSI: return copyFrom(X86::RSI);
    case ConstantReg::Kind::RDI: return copyFrom(X86::RDI);
    case ConstantReg::Kind::RBP: return copyFrom(X86::RBP);
    case ConstantReg::Kind::R8:  return copyFrom(X86::R8);
    case ConstantReg::Kind::R9:  return copyFrom(X86::R9);
    case ConstantReg::Kind::R10: return copyFrom(X86::R10);
    case ConstantReg::Kind::R11: return copyFrom(X86::R11);
    case ConstantReg::Kind::R12: return copyFrom(X86::R12);
    case ConstantReg::Kind::R13: return copyFrom(X86::R13);
    case ConstantReg::Kind::R14: return copyFrom(X86::R14);
    case ConstantReg::Kind::R15: return copyFrom(X86::R15);
    case ConstantReg::Kind::FS:  return copyFrom(X86::FS);
    // Program counter.
    case ConstantReg::Kind::PC: {
      auto &MMI = MF->getMMI();
      auto *label = MMI.getContext().createTempSymbol();
      CurDAG->setRoot(CurDAG->getEHLabel(SDL_, CurDAG->getRoot(), label));
      return CurDAG->getNode(
          X86ISD::WrapperRIP,
          SDL_,
          MVT::i64,
          CurDAG->getMCSymbol(label, MVT::i64)
      );
    }
    // Stack pointer.
    case ConstantReg::Kind::RSP: {
      return CurDAG->getNode(ISD::STACKSAVE, SDL_, MVT::i64, CurDAG->getRoot());
    }
    // Return address.
    case ConstantReg::Kind::RET_ADDR: {
      return CurDAG->getNode(
          ISD::RETURNADDR,
          SDL_,
          MVT::i64,
          CurDAG->getTargetConstant(0, SDL_, MVT::i64)
      );
    }
    // Frame address.
    case ConstantReg::Kind::FRAME_ADDR: {
      MF->getFrameInfo().setReturnAddressIsTaken(true);

      if (frameIndex_ == 0) {
        frameIndex_ = MF->getFrameInfo().CreateFixedObject(8, 0, false);
      }

      return CurDAG->getFrameIndex(frameIndex_, MVT::i64);
    }
  }
  llvm_unreachable("invalid register kind");
}

// -----------------------------------------------------------------------------
unsigned X86ISel::AssignVReg(const Inst *inst)
{
  MVT VT = GetType(inst->GetType(0));

  auto *RegInfo = &MF->getRegInfo();
  auto reg = RegInfo->createVirtualRegister(TLI->getRegClassFor(VT));

  regs_[inst] = reg;

  return reg;
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::LowerGlobal(const Global *val, int64_t offset)
{
  const std::string_view name = val->GetName();
  MVT ptrTy = MVT::i64;

  switch (val->GetKind()) {
    case Global::Kind::BLOCK: {
      auto *block = static_cast<const Block *>(val);
      auto *MBB = blocks_[block];

      auto *BB = const_cast<llvm::BasicBlock *>(MBB->getBasicBlock());
      auto *BA = llvm::BlockAddress::get(F, BB);

      return CurDAG->getBlockAddress(BA, ptrTy);
    }
    case Global::Kind::ATOM:
    case Global::Kind::FUNC:{
      auto *GV = M->getNamedValue(name.data());
      if (!GV) {
        llvm::report_fatal_error("Unknown symbol '" + std::string(name) + "'");
        break;
      }

      SDValue node;
      if (shared_ && !val->IsHidden()) {
        SDValue addr = CurDAG->getTargetGlobalAddress(
            GV,
            SDL_,
            ptrTy,
            0,
            llvm::X86II::MO_GOTPCREL
        );

        SDValue addrRIP = CurDAG->getNode(
            X86ISD::WrapperRIP,
            SDL_,
            ptrTy,
            addr
        );

        node = CurDAG->getLoad(
            ptrTy,
            SDL_,
            CurDAG->getEntryNode(),
            addrRIP,
            llvm::MachinePointerInfo::getGOT(CurDAG->getMachineFunction())
        );
      } else {
        node = CurDAG->getNode(
            X86ISD::WrapperRIP,
            SDL_,
            ptrTy, CurDAG->getTargetGlobalAddress(
                GV,
                SDL_,
                ptrTy,
                0,
                llvm::X86II::MO_NO_FLAG
            )
        );
      }

      if (offset == 0) {
        return node;
      } else {
        return CurDAG->getNode(
            ISD::ADD,
            SDL_,
            ptrTy,
            node,
            CurDAG->getConstant(offset, SDL_, ptrTy)
        );
      }
    }
    case Global::Kind::EXTERN: {
      if (auto *GV = M->getNamedValue(name.data())) {
        return CurDAG->getGlobalAddress(GV, SDL_, ptrTy, offset);
      } else {
        llvm::report_fatal_error("Unknown extern '" + std::string(name) + "'");
      }
    }
  }
  llvm_unreachable("invalid global type");
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *X86ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}

// -----------------------------------------------------------------------------
template<typename T>
void X86ISel::LowerCallSite(SDValue chain, const CallSite<T> *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->GetNumArgs() > call->GetNumFixedArgs();
  bool isTailCall = call->Is(Inst::Kind::TCALL) || call->Is(Inst::Kind::TINVOKE);
  bool isInvoke = call->Is(Inst::Kind::INVOKE) || call->Is(Inst::Kind::TINVOKE);
  bool wasTailCall = isTailCall;
  X86Call locs(call, isVarArg, isTailCall);

  // Find the number of bytes allocated to hold arguments.
  unsigned stackSize = locs.GetFrameSize();

  // Compute the stack difference for tail calls.
  int fpDiff = 0;
  if (isTailCall) {
    X86Call callee(func);
    int bytesToPop;
    switch (func->GetCallingConv()) {
      case CallingConv::C: {
        if (func->IsVarArg()) {
          bytesToPop = callee.GetFrameSize();
        } else {
          bytesToPop = 0;
        }
        break;
      }
      case CallingConv::SETJMP:
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC:
      case CallingConv::CAML_GC:
      case CallingConv::CAML_RAISE: {
        bytesToPop = 0;
        break;
      }
    }
    fpDiff = bytesToPop - static_cast<int>(stackSize);
  }

  if (isTailCall && fpDiff) {
    // TODO: some tail calls can still be lowered.
    wasTailCall = true;
    isTailCall = false;
  }

  // Calls from OCaml to C need to go through a trampoline.
  bool needsTrampoline = false;
  if (func->GetCallingConv() == CallingConv::CAML) {
    switch (call->GetCallingConv()) {
      case CallingConv::C:
        needsTrampoline = call->HasAnnot(CAML_FRAME) || call->HasAnnot(CAML_ROOT);
        break;
      case CallingConv::SETJMP:
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC:
      case CallingConv::CAML_GC:
      case CallingConv::CAML_RAISE:
        break;
    }
  }

  // Find the register mask, based on the calling convention.
  unsigned cc;
  {
    using namespace llvm::CallingConv;
    if (needsTrampoline) {
      cc = LLIR_CAML_EXT;
    } else {
      switch (call->GetCallingConv()) {
        case CallingConv::C:          cc = C;               break;
        case CallingConv::CAML:       cc = LLIR_CAML;       break;
        case CallingConv::CAML_ALLOC: cc = LLIR_CAML_ALLOC; break;
        case CallingConv::CAML_GC:    cc = LLIR_CAML_GC;    break;
        case CallingConv::CAML_RAISE: cc = LLIR_CAML_RAISE; break;
        case CallingConv::SETJMP:     cc = LLIR_SETJMP;     break;
      }
    }
  }
  const uint32_t *mask = mask = TRI_->getCallPreservedMask(*MF, cc);

  // Instruction bundle starting the call.
  chain = CurDAG->getCALLSEQ_START(chain, stackSize, 0, SDL_);

  // Generate a GC_FRAME before the call, if needed.
  std::vector<std::pair<const Inst *, SDValue>> frameExport;
  if (call->HasAnnot(CAML_ROOT)) {
    SDValue frameOps[] = { chain };
    auto *symbol = MMI.getContext().createTempSymbol();
    chain = CurDAG->getGCFrame(SDL_, ISD::ROOT, frameOps, symbol);
  } else if (call->HasAnnot(CAML_FRAME) && !isTailCall) {
    frameExport = GetFrameExport(call);

    // Allocate a reg mask which does not block the return register.
    uint32_t *frameMask = MF->allocateRegMask();
    unsigned maskSize = llvm::MachineOperand::getRegMaskSize(TRI_->getNumRegs());
    memcpy(frameMask, mask, sizeof(frameMask[0]) * maskSize);

    if (wasTailCall || !call->use_empty()) {
      if (auto retTy = call->GetType()) {
        // Find the physical reg where the return value is stored.
        unsigned retReg;
        switch (*retTy) {
          case Type::I8:  retReg = X86::AL;   break;
          case Type::I16: retReg = X86::AX;   break;
          case Type::I32: retReg = X86::EAX;  break;
          case Type::I64: retReg = X86::RAX;  break;
          case Type::F32: retReg = X86::XMM0; break;
          case Type::F64: retReg = X86::XMM0; break;
          case Type::F80: retReg = X86::FP0;  break;
          case Type::I128: {
            Error(call, "unsupported return value type");
          }
        }

        // Clear all subregs.
        for (llvm::MCSubRegIterator SR(retReg, TRI_, true); SR.isValid(); ++SR) {
          frameMask[*SR / 32] |= 1u << (*SR % 32);
        }
      }
    }

    llvm::SmallVector<SDValue, 8> frameOps;
    frameOps.push_back(chain);
    frameOps.push_back(CurDAG->getRegisterMask(frameMask));
    for (auto &[inst, val] : frameExport) {
      frameOps.push_back(val);
    }
    auto *symbol = MMI.getContext().createTempSymbol();
    chain = CurDAG->getGCFrame(SDL_, ISD::CALL, frameOps, symbol);
  }

  // Identify registers and stack locations holding the arguments.
  llvm::SmallVector<std::pair<unsigned, SDValue>, 8> regArgs;
  llvm::SmallVector<SDValue, 8> memOps;
  SDValue stackPtr;
  for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
    SDValue argument = GetValue(it->Value);
    switch (it->Kind) {
      case X86Call::Loc::Kind::REG: {
        regArgs.emplace_back(it->Reg, argument);
        break;
      }
      case X86Call::Loc::Kind::STK: {
        if (!stackPtr.getNode()) {
          stackPtr = CurDAG->getCopyFromReg(
              chain,
              SDL_,
              TRI_->getStackRegister(),
              ptrTy
          );
        }

        SDValue memOff = CurDAG->getNode(
            ISD::ADD,
            SDL_,
            ptrTy,
            stackPtr,
            CurDAG->getIntPtrConstant(it->Idx, SDL_)
        );

        memOps.push_back(CurDAG->getStore(
            chain,
            SDL_,
            argument,
            memOff,
            llvm::MachinePointerInfo::getStack(*MF, it->Idx)
        ));

        break;
      }
    }
  }

  if (!memOps.empty()) {
    chain = CurDAG->getNode(ISD::TokenFactor, SDL_, MVT::Other, memOps);
  }

  if (isVarArg) {
    // If XMM regs are used, their count needs to be passed in AL.
    unsigned count = 0;
    for (auto arg : call->args()) {
      if (IsFloatType(arg->GetType(0))) {
        count = std::min(8u, count + 1);
      }
    }

    regArgs.push_back({ X86::AL, CurDAG->getConstant(count, SDL_, MVT::i8) });
  }

  if (isTailCall) {
    // Shuffle arguments on the stack.
    for (auto it = locs.arg_begin(); it != locs.arg_end(); ++it) {
      switch (it->Kind) {
        case X86Call::Loc::Kind::REG: {
          continue;
        }
        case X86Call::Loc::Kind::STK: {
          assert(!"not implemented");
          break;
        }
      }
    }

    // Store the return address.
    if (fpDiff) {
      assert(!"not implemented");
    }
  }

  // Find the callee.
  SDValue callee;
  if (needsTrampoline) {
    // If call goes through a trampoline, replace the callee
    // and add the original one as the argument passed through $rax.
    if (!trampoline_) {
      trampoline_ = llvm::Function::Create(
          funcTy_,
          GlobalValue::ExternalLinkage,
          0,
          "caml_c_call",
          M
      );
    }
    regArgs.emplace_back(X86::RAX, GetValue(call->GetCallee()));
    callee = CurDAG->getTargetGlobalAddress(
        trampoline_,
        SDL_,
        MVT::i64,
        0,
        llvm::X86II::MO_NO_FLAG
    );
  } else {
    if (auto *movInst = ::dyn_cast_or_null<MovInst>(call->GetCallee())) {
      auto *movArg = movInst->GetArg();
      switch (movArg->GetKind()) {
        case Value::Kind::INST:
          callee = GetValue(static_cast<const Inst *>(movArg));
          break;

        case Value::Kind::GLOBAL: {
          auto *movGlobal = static_cast<const Global *>(movArg);
          switch (movGlobal->GetKind()) {
            case Global::Kind::BLOCK:
              llvm_unreachable("invalid call argument");

            case Global::Kind::FUNC:
            case Global::Kind::ATOM:
            case Global::Kind::EXTERN: {
              const std::string_view name = movGlobal->GetName();
              if (auto *GV = M->getNamedValue(name.data())) {
                callee = CurDAG->getTargetGlobalAddress(
                    GV,
                    SDL_,
                    MVT::i64,
                    0,
                    llvm::X86II::MO_NO_FLAG
                );
              } else {
                Error(call, "Unknown symbol '" + std::string(name) + "'");
              }
              break;
            }
          }
          break;
        }

        case Value::Kind::EXPR:
        case Value::Kind::CONST:
          llvm_unreachable("invalid call argument");
      }
    } else {
      callee = GetValue(call->GetCallee());
    }
  }

  // Prepare arguments in registers.
  SDValue inFlag;
  for (const auto &reg : regArgs) {
    chain = CurDAG->getCopyToReg(
        chain,
        SDL_,
        reg.first,
        reg.second,
        inFlag
    );
    inFlag = chain.getValue(1);
  }

  // Finish the call here for tail calls.
  if (isTailCall) {
    chain = CurDAG->getCALLSEQ_END(
        chain,
        CurDAG->getIntPtrConstant(stackSize, SDL_, true),
        CurDAG->getIntPtrConstant(0, SDL_, true),
        inFlag,
        SDL_
    );
    inFlag = chain.getValue(1);
  }

  // Create the DAG node for the Call.
  llvm::SmallVector<SDValue, 8> ops;
  ops.push_back(chain);
  ops.push_back(callee);
  if (isTailCall) {
    ops.push_back(CurDAG->getConstant(fpDiff, SDL_, MVT::i32));
  }
  for (const auto &reg : regArgs) {
    ops.push_back(CurDAG->getRegister(
        reg.first,
        reg.second.getValueType()
    ));
  }
  ops.push_back(CurDAG->getRegisterMask(mask));

  // Finalize the call node.
  if (inFlag.getNode()) {
    ops.push_back(inFlag);
  }

  // Generate a call or a tail call.
  SDVTList nodeTypes = CurDAG->getVTList(MVT::Other, MVT::Glue);
  if (isTailCall) {
    MF->getFrameInfo().setHasTailCall();
    CurDAG->setRoot(CurDAG->getNode(X86ISD::TC_RETURN, SDL_, nodeTypes, ops));
  } else {
    chain = CurDAG->getNode(X86ISD::CALL, SDL_, nodeTypes, ops);
    inFlag = chain.getValue(1);

    chain = CurDAG->getCALLSEQ_END(
        chain,
        CurDAG->getIntPtrConstant(stackSize, SDL_, true),
        CurDAG->getIntPtrConstant(0, SDL_, true),
        inFlag,
        SDL_
    );

    // Lower the return value.
    std::vector<SDValue> tailReturns;
    if (auto retTy = call->GetType()) {
      // Find the physical reg where the return value is stored.
      unsigned retReg;
      MVT retVT;
      switch (*retTy) {
        case Type::I8: {
          retReg = X86::AL;
          retVT = MVT::i8;
          break;
        }
        case Type::I16: {
          retReg = X86::AX;
          retVT = MVT::i16;
          break;
        }
        case Type::I32: {
          retReg = X86::EAX;
          retVT = MVT::i32;
          break;
        }
        case Type::I64: {
          retReg = X86::RAX;
          retVT = MVT::i64;
          break;
        }
        case Type::I128: {
          Error(call, "unsupported return value type");
        }
        case Type::F32: {
          retReg = X86::XMM0;
          retVT = MVT::f32;
          break;
        }
        case Type::F64: {
          retReg = X86::XMM0;
          retVT = MVT::f64;
          break;
        }
        case Type::F80: {
          retReg = X86::FP0;
          retVT = MVT::f80;
          break;
        }
      }

      if (wasTailCall || !isTailCall) {
        if (wasTailCall) {
          /// Copy the return value into a vreg.
          chain = CurDAG->getCopyFromReg(
              chain,
              SDL_,
              retReg,
              retVT,
              chain.getValue(1)
          ).getValue(1);

          /// If this was a tailcall, forward to return.
          tailReturns.push_back(chain.getValue(0));
        } else {
          // Regular call with a return which is used - expose it.
          if (!call->use_empty()) {
            chain = CurDAG->getCopyFromReg(
                chain,
                SDL_,
                retReg,
                retVT,
                chain.getValue(1)
            ).getValue(1);

            // Otherwise, expose the value.
            Export(call, chain.getValue(0));
          }

          // Ensure live values are not lifted before this point.
          if (!isInvoke) {
            for (auto &[inst, v] : frameExport) {
              chain = BreakVar(chain, inst, v);
            }
          }
        }
      }
    }

    if (wasTailCall) {
      llvm::SmallVector<SDValue, 6> returns;
      returns.push_back(chain);
      returns.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));
      for (auto &reg : liveOnExit_) {
        returns.push_back(CurDAG->getRegister(reg, MVT::i64));
      }

      for (auto &ret : tailReturns) {
        returns.push_back(ret);
      }

      chain = CurDAG->getNode(
          X86ISD::RET_FLAG,
          SDL_,
          MVT::Other,
          returns
      );
    }

    CurDAG->setRoot(chain);
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSyscall(const SyscallInst *inst)
{
  static unsigned kRegs[] = {
      X86::RDI, X86::RSI, X86::RDX,
      X86::R10, X86::R8, X86::R9
  };

  llvm::SmallVector<SDValue, 7> ops;
  SDValue chain = CurDAG->getRoot();

  // Lower arguments.
  unsigned args = 0;
  {
    unsigned n = sizeof(kRegs) / sizeof(kRegs[0]);
    for (const Inst *arg : inst->args()) {
      if (args >= n) {
        Error(inst, "too many arguments to syscall");
      }

      SDValue value = GetValue(arg);
      if (arg->GetType(0) != Type::I64) {
        Error(inst, "invalid syscall argument");
      }
      ops.push_back(CurDAG->getRegister(kRegs[args], MVT::i64));
      chain = CurDAG->getCopyToReg(chain, SDL_, kRegs[args++], value);
    }
  }

  /// Lower to the syscall.
  {
    ops.push_back(CurDAG->getRegister(X86::RAX, MVT::i64));

    chain = CurDAG->getCopyToReg(
        chain,
        SDL_,
        X86::RAX,
        GetValue(inst->GetSyscall())
    );

    ops.push_back(chain);

    chain = SDValue(CurDAG->getMachineNode(
        X86::SYSCALL,
        SDL_,
        CurDAG->getVTList(MVT::Other, MVT::Glue),
        ops
    ), 0);
  }

  /// Copy the return value into a vreg and export it.
  {
    if (inst->GetType() != Type::I64) {
      Error(inst, "invalid syscall type");
    }

    chain = CurDAG->getCopyFromReg(
        chain,
        SDL_,
        X86::RAX,
        MVT::i64,
        chain.getValue(1)
    ).getValue(1);

    Export(inst, chain.getValue(0));
  }

  CurDAG->setRoot(chain);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSwitch(const SwitchInst *inst)
{
  llvm::SelectionDAG &dag = GetDAG();
  llvm::MachineFunction &MF = dag.getMachineFunction();
  const llvm::TargetLowering &TLI = GetTargetLowering();

  auto *sourceMBB = blocks_[inst->getParent()];

  std::vector<llvm::MachineBasicBlock*> branches;
  for (unsigned i = 0, n = inst->getNumSuccessors(); i < n; ++i) {
    auto *mbb = blocks_[inst->getSuccessor(i)];
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

  auto *jti = MF.getOrCreateJumpTableInfo(TLI.getJumpTableEncoding());
  int jumpTableId = jti->createJumpTableIndex(branches);
  auto ptrTy = TLI.getPointerTy(dag.getDataLayout());

  SDValue jt = dag.getTargetJumpTable(
      jumpTableId,
      ptrTy,
      llvm::X86II::MO_NO_FLAG
  );

  jt = dag.getNode(
      X86ISD::WrapperRIP,
      SDL_,
      ptrTy,
      jt
  );

  dag.setRoot(dag.getNode(
      ISD::BR_JT,
      SDL_,
      MVT::Other,
      GetExportRoot(),
      jt,
      GetValue(inst->GetIdx())
  ));
}

// -----------------------------------------------------------------------------
std::vector<std::pair<const Inst *, SDValue>>
X86ISel::GetFrameExport(const Inst *frame)
{
  if (!lva_) {
    lva_.reset(new LiveVariables(func_));
  }

  std::vector<std::pair<const Inst *, SDValue>> exports;
  for (auto *inst : lva_->LiveOut(frame)) {
    if (!inst->HasAnnot(CAML_VALUE)) {
      continue;
    }
    if (inst == frame) {
      continue;
    }
    assert(inst->GetNumRets() == 1);
    assert(inst->GetType(0) == Type::I64);

    // Arg nodes which peek up the stack map to a memoperand.
    if (auto *argInst = ::dyn_cast_or_null<const ArgInst>(inst)) {
      auto &argLoc = (*conv_)[argInst->GetIdx()];
      switch (argLoc.Kind) {
        case X86Call::Loc::Kind::REG: {
          exports.emplace_back(inst, GetValue(inst));
          break;
        }
        case X86Call::Loc::Kind::STK: {
          int slot = args_[argLoc.Index];
          auto &MFI = MF->getFrameInfo();
          exports.emplace_back(inst, GetValue(inst));
          exports.emplace_back(inst, CurDAG->getGCArg(
              SDL_,
              MVT::i64,
              MF->getMachineMemOperand(
                  llvm::MachinePointerInfo::getFixedStack(
                      CurDAG->getMachineFunction(),
                      slot
                  ),
                  (
                    llvm::MachineMemOperand::MOLoad |
                    llvm::MachineMemOperand::MOStore
                  ),
                  MFI.getObjectSize(slot),
                  MFI.getObjectAlign(slot)
              )
          ));
          break;
        }
      }
    } else {
      // Constant values might be tagged as such, but are not GC roots.
      SDValue v = GetValue(inst);
      if (llvm::isa<GlobalAddressSDNode>(v) || llvm::isa<ConstantSDNode>(v)) {
        continue;
      }
      exports.emplace_back(inst, v);
    }

  }
  return exports;
}

// -----------------------------------------------------------------------------
llvm::SDValue X86ISel::BreakVar(SDValue chain, const Inst *inst, SDValue value)
{
  if (value->getOpcode() == ISD::GC_ARG) {
    return chain;
  }

  auto *RegInfo = &MF->getRegInfo();
  auto reg = RegInfo->createVirtualRegister(TLI->getRegClassFor(MVT::i64));
  chain = CurDAG->getCopyToReg(chain, SDL_, reg, value);
  chain = CurDAG->getCopyFromReg(
      chain,
      SDL_,
      reg,
      MVT::i64
  ).getValue(1);

  values_[inst] = chain.getValue(0);
  if (auto it = regs_.find(inst); it != regs_.end()) {
    if (auto jt = pendingExports_.find(it->second); jt != pendingExports_.end()) {
      jt->second = chain.getValue(0);
    }
  }

  return chain;
}

// -----------------------------------------------------------------------------
llvm::StringRef X86ISel::getPassName() const
{
  return "LLIR -> X86 DAG pass";
}

// -----------------------------------------------------------------------------
void X86ISel::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
  AU.addPreserved<llvm::MachineModuleInfoWrapperPass>();
}
