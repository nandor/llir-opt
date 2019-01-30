// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineJumpTableInfo.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/SelectionDAGISel.h>
#include <llvm/Target/X86/X86ISelLowering.h>

#include "core/block.h"
#include "core/data.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/symbol.h"
#include "emitter/x86/x86call.h"
#include "emitter/x86/x86isel.h"

namespace ISD = llvm::ISD;
namespace X86 = llvm::X86;
namespace X86ISD = llvm::X86ISD;
using MVT = llvm::MVT;
using EVT = llvm::EVT;
using SDNodeFlags = llvm::SDNodeFlags;
using SDNode = llvm::SDNode;
using SDValue = llvm::SDValue;
using SDVTList = llvm::SDVTList;
using SelectionDAG = llvm::SelectionDAG;
using X86RegisterInfo = llvm::X86RegisterInfo;



// -----------------------------------------------------------------------------
class ISelError final : public std::exception {
public:
  /// Constructs a new error object.
  ISelError(const Inst *i, const std::string_view &message)
  {
    auto block = i->getParent();
    auto func = block->getParent();

    std::ostringstream os;
    os << func->GetName() << "," << block->GetName() << ": " << message;
    message_ = os.str();
  }

  /// Returns the error message.
  const char *what() const noexcept
  {
    return message_.c_str();
  }

private:
  /// Error message.
  std::string message_;
};


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
  : DAGMatcher(*TM, new llvm::SelectionDAG(*TM, OL), OL, TLI, TII)
  , X86DAGMatcher(*TM, OL, STI)
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
  funcTy_ = llvm::FunctionType::get(voidTy_, {});

  // Create function definitions for all functions.
  for (const Func &func : *prog_) {
    auto *GV = M->getOrInsertFunction(func.GetName().data(), funcTy_);
    auto *F = llvm::dyn_cast<llvm::Function>(GV);
    // Set a dummy calling conv to emulate the set
    // of registers preserved by the callee.
    switch (func.GetCallingConv()) {
      case CallingConv::C:     F->setCallingConv(llvm::CallingConv::C);    break;
      case CallingConv::FAST:  F->setCallingConv(llvm::CallingConv::Fast); break;
      case CallingConv::OCAML: F->setCallingConv(llvm::CallingConv::GHC);  break;
      case CallingConv::EXT:   F->setCallingConv(llvm::CallingConv::GHC);  break;
    }
    llvm::BasicBlock* block = llvm::BasicBlock::Create(F->getContext(), "entry", F);
    llvm::IRBuilder<> builder(block);
    builder.CreateRetVoid();
  }

  // Create function declarations for externals.
  for (const Global *ext : prog_->externs()) {
    M->getOrInsertFunction(ext->GetName().data(), funcTy_);
  }

  // Add symbols for data values.
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
    F = M->getFunction(func.GetName().data());

    // Create a MachineFunction, attached to the dummy one.
    auto ORE = std::make_unique<llvm::OptimizationRemarkEmitter>(F);
    MF = &MMI.getOrCreateMachineFunction(*F);
    funcs_[&func] = MF;
    MF->setAlignment(func.GetAlignment());
    FuncInfo_ = MF->getInfo<llvm::X86MachineFunctionInfo>();

    // Initialise the dag with info for this function.
    CurDAG->init(*MF, *ORE, this, LibInfo_, nullptr);

    // Traverse nodes, entry first.
    llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);

    // Flag indicating if the function has VASTART.
    bool hasVAStart = false;

    // Create a MBB for all GenM blocks, isolating the entry block.
    const Block *entry = nullptr;
    llvm::MachineBasicBlock *entryMBB = nullptr;
    auto *RegInfo = &MF->getRegInfo();

    for (const Block *block : blockOrder) {
      // Create a skeleton basic block, with a jump to itself.
      llvm::BasicBlock *BB = llvm::BasicBlock::Create(
          M->getContext(),
          block->GetName().data(),
          F,
          nullptr
      );
      llvm::BranchInst::Create(BB, BB);

      // Create the basic block to be filled in by the instruction selector.
      llvm::MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(BB);
      MBB->setHasAddressTaken();
      blocks_[block] = MBB;
      MF->push_back(MBB);

      // First block in reverse post-order is the entry block.
      entry = entry ? entry : block;
      entryMBB = entryMBB ? entryMBB : MBB;

      // Allocate registers for exported values and create PHI
      // instructions for all PHI nodes in the basic block.
      for (const auto &inst : *block) {
        if (inst.Is(Inst::Kind::PHI)) {
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

      Chain = CurDAG->getRoot();
      {
        // If this is the entry block, lower all arguments.
        if (block == entry) {
          X86Call call(&func);
          if (hasVAStart) {
            LowerVASetup(func, call);
          }
          for (auto &argLoc : call.args()) {
            LowerArg(func, argLoc);
          }

          // Set the stack size of the new function.
          auto &MFI = MF->getFrameInfo();
          if (unsigned stackSize = func.GetStackSize()) {
            stackIndex_ = MFI.CreateStackObject(stackSize, 1, false);
          }
        }

        // Set up the SelectionDAG for the block.
        for (const auto &inst : *block) {
          Lower(&inst);
          if (inst.IsAnnotated()) {
            auto *symbol = MMI.getContext().createTempSymbol();
            Chain = CurDAG->getEHLabel(SDL_, Chain, symbol);
            labels_[&inst] = symbol;
          }
        }
      }
      CurDAG->setRoot(Chain);

      // Lower the block.
      insert_ = MBB_->end();
      CodeGenAndEmitDAG();
      ScheduleAnnotations(block, MBB_);

      // Clear values, except exported ones.
      values_.clear();
    }

    // If the entry block has a predecessor, insert a dummy entry.
    if (entryMBB->pred_size() != 0) {
      auto *block = MF->CreateMachineBasicBlock();

      CurDAG->setRoot(CurDAG->getNode(
          ISD::BR,
          SDL_,
          MVT::Other,
          CurDAG->getRoot(),
          CurDAG->getBasicBlock(entryMBB)
      ));

      insert_ = block->end();
      CodeGenAndEmitDAG();

      MF->push_front(block);
      block->addSuccessor(entryMBB);
      entryMBB = block;
    }

    // Emit copies from args into vregs at the entry.
    const auto &TRI = *MF->getSubtarget().getRegisterInfo();
    RegInfo->EmitLiveInCopies(entryMBB, TRI, *TII);

    TLI->finalizeLowering(*MF);

    DAGSize_ = 0;
    MBB_ = nullptr;
    MF = nullptr;
  }

  // Finalize lowering of references.
  if (auto *data = prog_->GetData()) {
    LowerRefs(data);
  }
  if (auto *bss = prog_->GetBSS()) {
    LowerRefs(bss);
  }
  if (auto *cst = prog_->GetConst()) {
    LowerRefs(cst);
  }

  return true;
}

// -----------------------------------------------------------------------------
void X86ISel::LowerData(const Data *data)
{
  for (const Atom &atom : *data) {
    auto *GV = new llvm::GlobalVariable(
        *M,
        voidTy_,
        false,
        llvm::GlobalValue::ExternalLinkage,
        nullptr,
        atom.GetName().data()
    );
    GV->setDSOLocal(true);
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerRefs(const Data *data)
{
  for (const Atom &atom : *data) {
    for (const Item *item : atom) {
      if (item->GetKind() != Item::Kind::SYMBOL) {
        continue;
      }
      if (!item->GetSymbol()->Is(Global::Kind::BLOCK)) {
        continue;
      }

      auto *block = static_cast<Block *>(item->GetSymbol());
      auto *MBB = blocks_[block];
      auto *BB = const_cast<llvm::BasicBlock *>(MBB->getBasicBlock());

      MBB->setHasAddressTaken();
      llvm::BlockAddress::get(BB->getParent(), BB);
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::Lower(const Inst *i)
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
    case Inst::Kind::XCHG:     return LowerXCHG(static_cast<const ExchangeInst *>(i));
    // Set register.
    case Inst::Kind::SET:      return LowerSet(static_cast<const SetInst *>(i));
    // Varargs.
    case Inst::Kind::VASTART:  return LowerVAStart(static_cast<const VAStartInst *>(i));
    // Constant.
    case Inst::Kind::FRAME:    return LowerFrame(static_cast<const FrameInst *>(i));
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
    case Inst::Kind::FEXT:     return LowerFExt(static_cast<const FExtInst *>(i));
    case Inst::Kind::MOV:      return LowerMov(static_cast<const MovInst *>(i));
    case Inst::Kind::TRUNC:    return LowerTrunc(static_cast<const TruncInst *>(i));
    // Binary instructions.
    case Inst::Kind::CMP:      return LowerCmp(static_cast<const CmpInst *>(i));
    case Inst::Kind::DIV:      return LowerBinary(i, ISD::SDIV, ISD::UDIV, ISD::FDIV);
    case Inst::Kind::REM:      return LowerBinary(i, ISD::SREM, ISD::UREM, ISD::FREM);
    case Inst::Kind::MUL:      return LowerBinary(i, ISD::MUL,  ISD::MUL,  ISD::FMUL);
    case Inst::Kind::ADD:      return LowerBinary(i, ISD::ADD,  ISD::ADD,  ISD::FADD);
    case Inst::Kind::SUB:      return LowerBinary(i, ISD::SUB,  ISD::SUB,  ISD::FSUB);
    case Inst::Kind::AND:      return LowerBinary(i, ISD::AND);
    case Inst::Kind::OR:       return LowerBinary(i, ISD::OR);
    case Inst::Kind::SLL:      return LowerBinary(i, ISD::SHL);
    case Inst::Kind::SRA:      return LowerBinary(i, ISD::SRA);
    case Inst::Kind::SRL:      return LowerBinary(i, ISD::SRL);
    case Inst::Kind::XOR:      return LowerBinary(i, ISD::XOR);
    case Inst::Kind::ROTL:     return LowerBinary(i, ISD::ROTL);
    case Inst::Kind::POW:      return LowerBinary(i, ISD::FPOW);
    case Inst::Kind::COPYSIGN: return LowerBinary(i, ISD::FCOPYSIGN);
    // Overflow checks.
    case Inst::Kind::UADDO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::UADDO);
    case Inst::Kind::UMULO:    return LowerALUO(static_cast<const OverflowInst *>(i), ISD::UMULO);
    // Undefined value.
    case Inst::Kind::UNDEF:    return LowerUndef(static_cast<const UndefInst *>(i));
    // Nodes handled separately.
    case Inst::Kind::PHI:      return;
    case Inst::Kind::ARG:      return;
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerBinary(const Inst *inst, unsigned op)
{
  auto *binaryInst = static_cast<const BinaryInst *>(inst);

  MVT type = GetType(binaryInst->GetType());
  SDValue lhs = GetValue(binaryInst->GetLHS());
  SDValue rhs = GetValue(binaryInst->GetRHS());
  SDValue binary = CurDAG->getNode(op, SDL_, type, lhs, rhs);
  Export(inst, binary);
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
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      LowerBinary(inst, sop);
      break;
    }
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128: {
      LowerBinary(inst, uop);
      break;
    }
    case Type::F32:
    case Type::F64: {
      LowerBinary(inst, fop);
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerUnary(const UnaryInst *inst, unsigned op)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  if (!IsFloatType(argTy) || !IsFloatType(retTy)) {
    throw std::runtime_error("unary insts operate on floats");
  }

  SDValue arg = GetValue(inst->GetArg());
  SDValue unary = CurDAG->getNode(op, SDL_, GetType(retTy), arg);
  Export(inst, unary);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJCC(const JumpCondInst *inst)
{
  auto *sourceMBB = blocks_[inst->getParent()];
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
  auto target = inst->GetTarget();
  if (target->GetType(0) != Type::I64) {
    throw std::runtime_error("invalid jump target");
  }

  Chain = CurDAG->getNode(
      ISD::BRIND,
      SDL_,
      MVT::Other,
      Chain,
      GetValue(target)
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerJMP(const JumpInst *inst)
{
  Block *target = inst->getSuccessor(0);
  auto *sourceMBB = blocks_[inst->getParent()];
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
  auto *sourceMBB = blocks_[inst->getParent()];

  std::vector<llvm::MachineBasicBlock*> branches;
  for (unsigned i = 0; i < inst->getNumSuccessors(); ++i) {
    auto *mbb = blocks_[inst->getSuccessor(i)];
    branches.push_back(mbb);
    sourceMBB->addSuccessor(mbb);
  }

  auto *jti = MF->getOrCreateJumpTableInfo(TLI->getJumpTableEncoding());
  int jumpTableId = jti->createJumpTableIndex(branches);
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());

  Chain = CurDAG->getNode(
      ISD::BR_JT,
      SDL_,
      MVT::Other,
      Chain,
      CurDAG->getJumpTable(jumpTableId, ptrTy),
      GetValue(inst->GetIdx())
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerLD(const LoadInst *ld)
{
  bool sign;
  size_t size;
  bool fp;
  switch (ld->GetType()) {
    case Type::I8:   fp = 0; sign = 1; size = 1;  break;
    case Type::I16:  fp = 0; sign = 1; size = 2;  break;
    case Type::I32:  fp = 0; sign = 1; size = 4;  break;
    case Type::I64:  fp = 0; sign = 1; size = 8;  break;
    case Type::I128: fp = 0; sign = 1; size = 16; break;
    case Type::U8:   fp = 0; sign = 0; size = 1;  break;
    case Type::U16:  fp = 0; sign = 0; size = 2;  break;
    case Type::U32:  fp = 0; sign = 0; size = 4;  break;
    case Type::U64:  fp = 0; sign = 0; size = 8;  break;
    case Type::U128: fp = 0; sign = 0; size = 16; break;
    case Type::F32:  fp = 1; sign = 0; size = 4;  break;
    case Type::F64:  fp = 1; sign = 0; size = 8;  break;
  }

  ISD::LoadExtType ext;
  if (size > ld->GetLoadSize()) {
    ext = sign ? ISD::SEXTLOAD : ISD::ZEXTLOAD;
  } else {
    ext = ISD::NON_EXTLOAD;
  }

  MVT mt;
  switch (auto sz = ld->GetLoadSize()) {
    case 1: mt = MVT::i8;  break;
    case 2: mt = MVT::i16; break;
    case 4: mt = fp ? MVT::f32 : MVT::i32; break;
    case 8: mt = fp ? MVT::f64 : MVT::i64; break;
    default: throw std::runtime_error("Load too large");
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
  Export(ld, l);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerST(const StoreInst *st)
{
  auto *val = st->GetVal();

  MVT mt;
  switch (st->GetStoreSize()) {
    case 1: mt = MVT::i8;  break;
    case 2: mt = MVT::i16; break;
    case 4: mt = val->GetType(0) == Type::F32 ? MVT::f32 : MVT::i32; break;
    case 8: mt = val->GetType(0) == Type::F64 ? MVT::f64 : MVT::i64; break;
    default: throw std::runtime_error("Store too large");
  }

  Chain = CurDAG->getTruncStore(
      Chain,
      SDL_,
      GetValue(val),
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
      case Type::I64: case Type::U64: retReg = X86::RAX; break;
      case Type::I32: case Type::U32: retReg = X86::EAX; break;
      case Type::F32: case Type::F64: retReg = X86::XMM0; break;
      default: throw std::runtime_error("Invalid return type");
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
  LowerCallSite(inst);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTailCall(const TailCallInst *inst)
{
  LowerCallSite(inst);
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

  // Lower the invoke call.
  LowerCallSite(inst);

  // Add a jump to the continuation block.
  Chain = CurDAG->getNode(
      ISD::BR,
      SDL_,
      MVT::Other,
      Chain,
      CurDAG->getBasicBlock(mbbCont)
  );

  // Mark successors.
  auto *sourceMBB = blocks_[inst->getParent()];
  sourceMBB->addSuccessor(mbbCont);
  sourceMBB->addSuccessor(mbbThrow);
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
  LowerCallSite(inst);

  // Mark successors.
  auto *sourceMBB = blocks_[inst->getParent()];
  sourceMBB->addSuccessor(mbbThrow);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFrame(const FrameInst *inst)
{
  SDValue base = CurDAG->getFrameIndex(stackIndex_, MVT::i64);
  SDValue index = CurDAG->getNode(
      ISD::ADD,
      SDL_,
      MVT::i64,
      base,
      CurDAG->getConstant(inst->GetIdx(), SDL_, MVT::i64)
  );
  Export(inst, index);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerCmp(const CmpInst *cmpInst)
{
  MVT type = GetType(cmpInst->GetType());
  SDValue lhs = GetValue(cmpInst->GetLHS());
  SDValue rhs = GetValue(cmpInst->GetRHS());
  ISD::CondCode cc = GetCond(cmpInst->GetCC());
  SDValue cmp = CurDAG->getSetCC(SDL_, MVT::i8, lhs, rhs, cc);
  SDValue flag = CurDAG->getZExtOrTrunc(cmp, SDL_, type);
  Export(cmpInst, flag);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTrap(const TrapInst *inst)
{
  Chain = CurDAG->getNode(ISD::TRAP, SDL_, MVT::Other, Chain);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerMov(const MovInst *inst)
{
  Type retType = inst->GetType();

  auto emitSymbol = [this, inst](Global *val, int64_t offset) {
    const std::string_view name = val->GetName();
    if (auto *GV = M->getNamedValue(name.data())) {
      Export(inst, CurDAG->getGlobalAddress(GV, SDL_, MVT::i64, offset));
    } else {
      throw std::runtime_error("Unknown symbol: " + std::string(name));
    }
  };

  auto *val = inst->GetArg();
  switch (val->GetKind()) {
    case Value::Kind::INST: {
      auto *arg = static_cast<Inst *>(val);
      SDValue argNode = GetValue(arg);
      Type argType = arg->GetType(0);
      if (argType == retType) {
        Export(inst, argNode);
      } else if (GetSize(argType) == GetSize(retType)) {
        Export(inst, CurDAG->getBitcast(GetType(retType), argNode));
      } else {
        throw std::runtime_error("unsupported mov");
      }
      break;
    }
    case Value::Kind::CONST: {
      switch (static_cast<Constant *>(val)->GetKind()) {
        case Constant::Kind::REG: {
          switch (static_cast<ConstantReg *>(val)->GetValue()) {
            case ConstantReg::Kind::SP: {
              Export(inst, CurDAG->getCopyFromReg(
                  Chain,
                  SDL_,
                  X86::RSP,
                  MVT::i64
              ));
              break;
            }
            case ConstantReg::Kind::RET_ADDR: {
              Export(inst, CurDAG->getNode(
                  ISD::RETURNADDR,
                  SDL_,
                  MVT::i64,
                  CurDAG->getTargetConstant(0, SDL_, MVT::i64)
              ));
              break;
            }
            case ConstantReg::Kind::FRAME_ADDR: {
              Export(inst, CurDAG->getNode(
                  ISD::ADD,
                  SDL_,
                  MVT::i64,
                  CurDAG->getNode(
                      ISD::ADDROFRETURNADDR,
                      SDL_,
                      MVT::i64,
                      CurDAG->getTargetConstant(0, SDL_, MVT::i64)
                  ),
                  CurDAG->getTargetConstant(8, SDL_, MVT::i64)
              ));
              break;
            }
          }
          break;
        }
        case Constant::Kind::INT: {
          Export(inst, LowerImm(
              ImmValue(static_cast<ConstantInt *>(val)->GetValue()),
              retType
          ));
          break;
        }
        case Constant::Kind::FLOAT: {
          Export(inst, LowerImm(
              ImmValue(static_cast<ConstantFloat *>(val)->GetValue()),
              retType
          ));
          break;
        }
      }
      break;
    }
    case Value::Kind::GLOBAL: {
      if (inst->GetType() != Type::I64) {
        throw std::runtime_error("Invalid address type");
      }

      switch (static_cast<Global *>(val)->GetKind()) {
        case Global::Kind::BLOCK: {
          auto *block = static_cast<Block *>(val);
          auto *MBB = blocks_[block];

          auto *BB = const_cast<llvm::BasicBlock *>(MBB->getBasicBlock());
          auto *BA = llvm::BlockAddress::get(F, BB);

          Export(inst, CurDAG->getBlockAddress(BA, MVT::i64));
          break;
        }
        case Global::Kind::ATOM:
        case Global::Kind::FUNC:
        case Global::Kind::EXTERN:
        case Global::Kind::SYMBOL: {
          emitSymbol(static_cast<Global *>(val), 0);
          break;
        }
      }
      break;
    }
    case Value::Kind::EXPR: {
      if (inst->GetType() != Type::I64) {
        throw std::runtime_error("Invalid address type");
      }
      switch (static_cast<Expr *>(val)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *symOff = static_cast<SymbolOffsetExpr *>(val);
          emitSymbol(symOff->GetSymbol(), symOff->GetOffset());
          break;
        }
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSExt(const SExtInst *inst)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  if (!IsIntegerType(argTy)) {
    throw std::runtime_error("sext requires integer argument");
  }

  unsigned opcode;
  if (IsIntegerType(retTy)) {
    opcode = ISD::SIGN_EXTEND;
  } else {
    opcode = ISD::SINT_TO_FP;
  }

  SDValue arg = GetValue(inst->GetArg());
  SDValue fext = CurDAG->getNode(opcode, SDL_, GetType(retTy), arg);
  Export(inst, fext);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerZExt(const ZExtInst *inst)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  if (!IsIntegerType(argTy)) {
    throw std::runtime_error("zext requires integer argument");
  }

  unsigned opcode;
  if (IsIntegerType(retTy)) {
    opcode = ISD::ZERO_EXTEND;
  } else {
    opcode = ISD::UINT_TO_FP;
  }

  SDValue arg = GetValue(inst->GetArg());
  SDValue fext = CurDAG->getNode(opcode, SDL_, GetType(retTy), arg);
  Export(inst, fext);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerFExt(const FExtInst *inst)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  if (!IsFloatType(argTy)) {
    throw ISelError(inst, "argument not a float");
  }
  if (!IsFloatType(retTy)) {
    throw ISelError(inst, "return not a float");
  }
  if (GetSize(argTy) >= GetSize(retTy)) {
    throw ISelError(inst, "Cannot shrink argument");
  }

  SDValue arg = GetValue(inst->GetArg());
  SDValue fext = CurDAG->getNode(ISD::FP_EXTEND, SDL_, GetType(retTy), arg);
  Export(inst, fext);
}

// -----------------------------------------------------------------------------
void X86ISel::LowerTrunc(const TruncInst *inst)
{
  Type argTy = inst->GetArg()->GetType(0);
  Type retTy = inst->GetType();

  MVT type = GetType(retTy);
  SDValue arg = GetValue(inst->GetArg());
  MVT ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());

  unsigned opcode;
  switch (retTy) {
    case Type::F32:
    case Type::F64: {
      if (IsIntegerType(argTy)) {
        throw std::runtime_error("Invalid truncate");
      } else {
        Export(inst, CurDAG->getNode(
            ISD::FP_ROUND,
            SDL_,
            type,
            arg,
            CurDAG->getTargetConstant(0, SDL_, ptrTy)
        ));
      }
      break;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      if (IsIntegerType(argTy)) {
        Export(inst, CurDAG->getNode(ISD::TRUNCATE, SDL_, type, arg));
      } else {
        Export(inst, CurDAG->getNode(ISD::FP_TO_SINT, SDL_, type, arg));
      }
      break;
    }
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128: {
      if (IsIntegerType(argTy)) {
        Export(inst, CurDAG->getNode(ISD::TRUNCATE, SDL_, type, arg));
      } else {
        Export(inst, CurDAG->getNode(ISD::FP_TO_UINT, SDL_, type, arg));
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerXCHG(const ExchangeInst *inst)
{
  auto *mmo = MF->getMachineMemOperand(
      llvm::MachinePointerInfo(static_cast<llvm::Value *>(nullptr)),
      llvm::MachineMemOperand::MOVolatile |
      llvm::MachineMemOperand::MOLoad |
      llvm::MachineMemOperand::MOStore,
      GetSize(inst->GetType()),
      GetSize(inst->GetType()),
      llvm::AAMDNodes(),
      nullptr,
      llvm::SyncScope::System,
      llvm::AtomicOrdering::SequentiallyConsistent,
      llvm::AtomicOrdering::SequentiallyConsistent
  );

  SDValue xchg = CurDAG->getAtomic(
      ISD::ATOMIC_SWAP,
      SDL_,
      GetType(inst->GetType()),
      Chain,
      GetValue(inst->GetAddr()),
      GetValue(inst->GetVal()),
      mmo
  );

  Chain = xchg.getValue(1);
  Export(inst, xchg.getValue(0));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSet(const SetInst *inst)
{
  auto value = GetValue(inst->GetValue());
  switch (inst->GetReg()->GetValue()) {
    case ConstantReg::Kind::SP: {
      Chain = CurDAG->getNode(
          ISD::STACKRESTORE,
          SDL_,
          MVT::Other,
          Chain,
          value
      );
      break;
    }
    case ConstantReg::Kind::FRAME_ADDR:
    case ConstantReg::Kind::RET_ADDR:
    {
      throw ISelError(inst, "Cannot assign to read-only register");
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerSelect(const SelectInst *select)
{
  SDValue node = CurDAG->getNode(
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
void X86ISel::LowerVAStart(const VAStartInst *inst)
{
  if (!inst->getParent()->getParent()->IsVarArg()) {
    throw ISelError(inst, "vastart in a non-vararg function");
  }

  Chain = CurDAG->getNode(
      ISD::VASTART,
      SDL_,
      MVT::Other,
      Chain,
      GetValue(inst->GetVAList()),
      CurDAG->getSrcValue(nullptr)
  );
}

// -----------------------------------------------------------------------------
void X86ISel::LowerUndef(const UndefInst *inst)
{
  Export(inst, CurDAG->getUNDEF(GetType(inst->GetType())));
}

// -----------------------------------------------------------------------------
void X86ISel::LowerALUO(const OverflowInst *inst, unsigned op)
{
  MVT type = GetType(inst->GetLHS()->GetType(0));
  SDValue lhs = GetValue(inst->GetLHS());
  SDValue rhs = GetValue(inst->GetRHS());

  SDVTList types = CurDAG->getVTList(type, MVT::i1);
  SDValue node = CurDAG->getNode(op, SDL_, types, lhs, rhs);
  SDValue flag = CurDAG->getZExtOrTrunc(node.getValue(1), SDL_, MVT::i32);

  Export(inst, flag);
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
      const auto *val = phi.GetValue(block);
      unsigned reg = 0;
      Type phiType = phi.GetType();
      MVT VT = GetType(phiType);

      switch (val->GetKind()) {
        case Value::Kind::INST: {
          auto *inst = static_cast<const Inst *>(val);
          auto it = regs_.find(inst);
          if (it != regs_.end()) {
            reg = it->second;
          } else {
            throw std::runtime_error("Invalid incoming vreg to PHI.");
          }
          break;
        }
        case Value::Kind::GLOBAL:
        case Value::Kind::EXPR: {
          throw std::runtime_error("Invalid incoming address to PHI.");
        }
        case Value::Kind::CONST: {
          SDValue value;
          switch (static_cast<const Constant *>(val)->GetKind()) {
            case Constant::Kind::INT: {
              value = LowerImm(
                  ImmValue(static_cast<const ConstantInt *>(val)->GetValue()),
                  phiType
              );
              break;
            }
            case Constant::Kind::FLOAT: {
              value = LowerImm(
                  ImmValue(static_cast<const ConstantFloat *>(val)->GetValue()),
                  phiType
              );
              break;
            }
            case Constant::Kind::REG: {
              throw std::runtime_error("Invalid incoming register to PHI.");
            }
          }
          reg = RegInfo->createVirtualRegister(TLI->getRegClassFor(VT));
          Chain = CurDAG->getCopyToReg(Chain, SDL_, reg, value);
          break;
        }
      }
      mPhi.addReg(reg).addMBB(blockMBB);
    }
  }
}

// -----------------------------------------------------------------------------
void X86ISel::LowerArg(const Func &func, X86Call::Loc &argLoc)
{
  const llvm::TargetRegisterClass *RC;
  MVT RegVT;
  switch (argLoc.Type) {
    case Type::U8:  case Type::I8:
    case Type::U16: case Type::I16:
    case Type::U128: case Type::I128: {
      throw std::runtime_error("Invalid argument to call.");
    }
    case Type::U32: case Type::I32: {
      RegVT = MVT::i32;
      RC = &X86::GR32RegClass;
      break;
    }
    case Type::U64: case Type::I64: {
      RegVT = MVT::i64;
      RC = &X86::GR64RegClass;
      break;
    }
    case Type::F32: {
      RegVT = MVT::f32;
      RC = &X86::FR32RegClass;
      break;
    }
    case Type::F64: {
      RegVT = MVT::f64;
      RC = &X86::FR64RegClass;
      break;
    }
  }

  unsigned Reg = MF->addLiveIn(argLoc.Reg, RC);
  SDValue arg = CurDAG->getCopyFromReg(Chain, SDL_, Reg, RegVT);

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

  // Get the size of the stack, plus alignment to store the return
  // address for tail calls for the fast calling convention.
  unsigned stackSize = ci.GetFrameSize();
  switch (func.GetCallingConv()) {
    case CallingConv::C: {
      break;
    }
    case CallingConv::FAST: {
      assert(!"not implemented");
      break;
    }
    case CallingConv::OCAML: {
      throw std::runtime_error("vararg call not supported in OCaml.");
    }
    case CallingConv::EXT: {
      throw std::runtime_error("vararg call not supported for external calls");
    }
  }

  FuncInfo_->setVarArgsFrameIndex(MFI.CreateFixedObject(1, stackSize, true));

  // Copy all unused regs to be pushed on the stack into vregs.
  llvm::SmallVector<SDValue, 6> liveGPRs;
  llvm::SmallVector<SDValue, 8> liveXMMs;
  SDValue alReg;

  for (unsigned reg : ci.GetUnusedGPRs()) {
    unsigned vreg = MF->addLiveIn(reg, &X86::GR64RegClass);
    liveGPRs.push_back(CurDAG->getCopyFromReg(Chain, SDL_, vreg, MVT::i64));
  }

  for (unsigned reg : ci.GetUnusedXMMs()) {
    if (!alReg) {
      unsigned vreg = MF->addLiveIn(X86::AL, &X86::GR8RegClass);
      alReg = CurDAG->getCopyFromReg(Chain, SDL_, vreg, MVT::i8);
    }
    unsigned vreg = MF->addLiveIn(reg, &X86::VR128RegClass);
    liveXMMs.push_back(CurDAG->getCopyFromReg(Chain, SDL_, vreg, MVT::v4f32));
  }

  // Save the indices to be stored in __va_list_tag
  unsigned numGPRs = ci.GetUnusedGPRs().size() + ci.GetUsedGPRs().size();
  unsigned numXMMs = ci.GetUnusedXMMs().size() + ci.GetUsedXMMs().size();
  FuncInfo_->setVarArgsGPOffset(ci.GetUsedGPRs().size() * 8);
  FuncInfo_->setVarArgsFPOffset(numGPRs * 8 + ci.GetUsedXMMs().size() * 16);
  FuncInfo_->setRegSaveFrameIndex(MFI.CreateStackObject(
      numGPRs * 8 + numXMMs * 16,
      16,
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
    ops.push_back(Chain);
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
    Chain = CurDAG->getNode(ISD::TokenFactor, SDL_, MVT::Other, storeOps);
  }
}

// -----------------------------------------------------------------------------
void X86ISel::Export(const Inst *inst, SDValue val)
{
  values_[inst] = val;
  auto it = regs_.find(inst);
  if (it == regs_.end()) {
    return;
  }
  Chain = CurDAG->getCopyToReg(Chain, SDL_, it->second, val);
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
  } else {
    throw ISelError(inst, "undefined virtual register");
  }
}

// -----------------------------------------------------------------------------
llvm::MVT X86ISel::GetType(Type t)
{
  switch (t) {
    case Type::I8:   return MVT::i8;
    case Type::I16:  return MVT::i16;
    case Type::I32:  return MVT::i32;
    case Type::I64:  return MVT::i64;
    case Type::I128: return MVT::i128;
    case Type::U8:   return MVT::i8;
    case Type::U16:  return MVT::i16;
    case Type::U32:  return MVT::i32;
    case Type::U64:  return MVT::i64;
    case Type::U128: return MVT::i128;
    case Type::F32:  return MVT::f32;
    case Type::F64:  return MVT::f64;
  }
}

// -----------------------------------------------------------------------------
ISD::CondCode X86ISel::GetCond(Cond cc)
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
void X86ISel::ScheduleAnnotations(
    const Block *block,
    llvm::MachineBasicBlock *MBB)
{
  for (auto &inst : *block) {
    if (!inst.IsAnnotated()) {
      continue;
    }

    // Labels can be placed after call sites, succeeded stack adjustment
    // and spill-restore instructions. This step adjusts label positions:
    // finds the EH_LABEL, removes it and inserts it after the preceding call.
    auto *label = labels_[&inst];
    for (auto it = MBB->begin(); it != MBB->end(); ++it) {
      if (it->isEHLabel() && it->getOperand(0).getMCSymbol() == label) {
        auto jt = it;
        do { jt--; } while (!jt->isCall());
        auto *MI = it->removeFromParent();
        MBB->insertAfter(jt, MI);
        break;
      }
    }
  }
}

// -----------------------------------------------------------------------------
llvm::ScheduleDAGSDNodes *X86ISel::CreateScheduler()
{
  return createILPListDAGScheduler(MF, TII, TRI_, TLI, OptLevel);
}

// -----------------------------------------------------------------------------
SDValue X86ISel::LowerImm(ImmValue val, Type type)
{
  switch (type) {
    case Type::U8:  case Type::I8: {
      return CurDAG->getConstant(val.i8v, SDL_, MVT::i8);
    }
    case Type::I16: case Type::U16: {
      return CurDAG->getConstant(val.i16v, SDL_, MVT::i16);
    }
    case Type::I32: case Type::U32: {
      return CurDAG->getConstant(val.i32v, SDL_, MVT::i32);
    }
    case Type::I64: case Type::U64: {
      return CurDAG->getConstant(val.i64v, SDL_, MVT::i64);
    }
    case Type::I128: case Type::U128: {
      llvm_unreachable("not implemented");
    }
    case Type::F32:{
      return CurDAG->getConstantFP(val.f32v, SDL_, MVT::f32);
    }
    case Type::F64: {
      return CurDAG->getConstantFP(val.f64v, SDL_, MVT::f64);
    }
  }
}

// -----------------------------------------------------------------------------
template<typename T>
void X86ISel::LowerCallSite(const CallSite<T> *call)
{
  const Block *block = call->getParent();
  const Func *func = block->getParent();
  auto ptrTy = TLI->getPointerTy(CurDAG->getDataLayout());

  // Analyse the arguments, finding registers for them.
  bool isVarArg = call->GetNumArgs() > call->GetNumFixedArgs();
  bool isTailCall = call->Is(Inst::Kind::TCALL) || call->Is(Inst::Kind::TINVOKE);
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
        bytesToPop = 0;
        break;
      }
      case CallingConv::EXT:
      case CallingConv::OCAML:
      case CallingConv::FAST: {
        if (func->IsVarArg()) {
          bytesToPop = callee.GetFrameSize();
        } else {
          bytesToPop = 0;
        }
        break;
      }
    }
    fpDiff = bytesToPop - static_cast<int>(stackSize);
  }

  // Instruction bundle starting the call.
  Chain = CurDAG->getCALLSEQ_START(Chain, stackSize, 0, SDL_);

  if (isTailCall && fpDiff) {
    // TODO: some tail calls can still be lowered.
    wasTailCall = true;
    isTailCall = false;
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
              Chain,
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
            Chain,
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
    Chain = CurDAG->getNode(ISD::TokenFactor, SDL_, MVT::Other, memOps);
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

  SDValue inFlag;
  for (const auto &reg : regArgs) {
    Chain = CurDAG->getCopyToReg(
        Chain,
        SDL_,
        reg.first,
        reg.second,
        inFlag
    );
    inFlag = Chain.getValue(1);
  }

  // Find the callee.
  SDValue callee = GetValue(call->GetCallee());

  // If the callee is a global address, lower it to a target global address
  // since the default LowerGlobalAddress generated a different instruction.
  if (callee->getOpcode() == ISD::GlobalAddress) {
    auto* G = llvm::cast<llvm::GlobalAddressSDNode>(callee);
    const llvm::GlobalValue *GV = G->getGlobal();

    callee = CurDAG->getTargetGlobalAddress(
        GV,
        SDL_,
        ptrTy,
        G->getOffset(),
        llvm::X86II::MO_NO_FLAG
    );
  }

  // Finish the call here for tail calls.
  if (isTailCall) {
    Chain = CurDAG->getCALLSEQ_END(
        Chain,
        CurDAG->getIntPtrConstant(stackSize, SDL_, true),
        CurDAG->getIntPtrConstant(0, SDL_, true),
        inFlag,
        SDL_
    );
    inFlag = Chain.getValue(1);
  }

  // Create the DAG node for the Call.
  llvm::SmallVector<SDValue, 8> ops;
  ops.push_back(Chain);
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

  // Find the register mask, based on the calling convention.
  const uint32_t *regMask = nullptr;
  switch (call->GetCallingConv()) {
    case CallingConv::C: {
      regMask = TRI_->getCallPreservedMask(*MF, llvm::CallingConv::C);
      break;
    }
    case CallingConv::FAST: {
      regMask = TRI_->getCallPreservedMask(*MF, llvm::CallingConv::C);
      break;
    }
    case CallingConv::OCAML: {
      regMask = TRI_->getNoPreservedMask();
      break;
    }
    case CallingConv::EXT: {
      regMask = TRI_->getNoPreservedMask();
      break;
    }
  }
  ops.push_back(CurDAG->getRegisterMask(regMask));

  // Finalize the call node.
  if (inFlag.getNode()) {
    ops.push_back(inFlag);
  }

  SDVTList nodeTypes = CurDAG->getVTList(MVT::Other, MVT::Glue);
  if (isTailCall) {
    MF->getFrameInfo().setHasTailCall();
    Chain = CurDAG->getNode(X86ISD::TC_RETURN, SDL_, nodeTypes, ops);
  } else {
    Chain = CurDAG->getNode(X86ISD::CALL, SDL_, nodeTypes, ops);
    inFlag = Chain.getValue(1);

    Chain = CurDAG->getCALLSEQ_END(
        Chain,
        CurDAG->getIntPtrConstant(stackSize, SDL_, true),
        CurDAG->getIntPtrConstant(0, SDL_, true),
        inFlag,
        SDL_
    );

    // Lower the return value.
    if (call->GetNumRets()) {
      inFlag = Chain.getValue(1);

      // Find the physical reg where the return value is stored.
      unsigned retReg;
      MVT retVT;
      switch (call->GetType(0)) {
        case Type::I8:  case Type::U8:
        case Type::I16: case Type::U16:
        case Type::I128: case Type::U128: {
          throw std::runtime_error("unsupported return value type");
        }
        case Type::I32: case Type::U32: {
          retReg = X86::EAX;
          retVT = MVT::i32;
          break;
        }
        case Type::I64: case Type::U64: {
          retReg = X86::RAX;
          retVT = MVT::i64;
          break;
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
      }

      /// Copy the return value into a vreg.
      Chain = CurDAG->getCopyFromReg(
          Chain,
          SDL_,
          retReg,
          retVT,
          inFlag
      ).getValue(1);

      SDValue retVal = Chain.getValue(0);

      Export(call, retVal);

      // If the tail call was not lowered, a return is required.
      if (wasTailCall) {
        llvm::SmallVector<SDValue, 6> returns;
        returns.push_back(Chain);
        returns.push_back(CurDAG->getTargetConstant(0, SDL_, MVT::i32));
        Chain = CurDAG->getNode(X86ISD::RET_FLAG, SDL_, MVT::Other, returns);
      }
    }
  }
}

// -----------------------------------------------------------------------------
llvm::StringRef X86ISel::getPassName() const
{
  return "GenM -> X86 DAG pass";
}

// -----------------------------------------------------------------------------
void X86ISel::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfo>();
  AU.addPreserved<llvm::MachineModuleInfo>();
}
