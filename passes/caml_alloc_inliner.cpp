// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/block.h"
#include "core/prog.h"
#include "core/func.h"
#include "core/insts.h"
#include "passes/caml_alloc_inliner.h"



// -----------------------------------------------------------------------------
const char *CamlAllocInlinerPass::kPassID = "caml-alloc-inliner";

// -----------------------------------------------------------------------------
static void InlineCall(CallSite *call, Block *cont, Block *raise)
{
  Block *block = call->getParent();
  Func *func = block->getParent();
  Prog *prog = func->getParent();

  // Identify calls to globals.
  auto movInst = ::cast_or_null<MovInst>(call->GetCallee());
  if (!movInst) {
    return;
  }
  auto movGlobal = ::cast_or_null<Global>(movInst->GetArg());
  if (!movGlobal) {
    return;
  }

  // Call must receive two arguments: Caml_state and Caml_state->young_ptr.
  Ref<Inst> statePtr = call->arg(0);
  Ref<Inst> youngPtr = call->arg(1);
  Ref<Inst> youngLimit = call->arg_size() > 2 ? call->arg(2) : nullptr;
  Ref<Inst> exnPtr = call->arg_size() > 3 ? call->arg(3) : nullptr;

  // Find the byte adjustment to insert.
  std::optional<unsigned> bytes;
  if (movGlobal->getName() == "caml_alloc1") {
    bytes = 16;
  } else if (movGlobal->getName() == "caml_alloc2") {
    bytes = 24;
  } else if (movGlobal->getName() == "caml_alloc3") {
    bytes = 32;
  } else if (movGlobal->getName() == "caml_allocN") {
    bytes = {};
  } else {
    return;
  }

  // Insert the byte adjustment at the end of the call block.
  if (bytes) {
    auto *constInst = new MovInst(Type::I64, new ConstantInt(*bytes), {});
    block->AddInst(constInst);
    auto *addInst = new SubInst(Type::I64, youngPtr, constInst, {});
    block->AddInst(addInst);
    youngPtr = addInst;
  }

  // Prepare phis in the no-collection branch.
  //
  //
  // Originally, a call looks like:
  //
  //   call.caml_alloc.i64.i64   $state, $ptr, fn, $old_state, $old_ptr, .L
  //.L:
  //   ... use $state, $ptr ...
  //
  // The call is changed into:
  //
  //.Lsrc:
  //  add.i64      $new_ptr, $old_ptr
  //  ld.i64       $young_limit, [$state_ptr + 8]
  //  cmp.uge.i8   $flag, $new_ptr, $young_limit
  //  jcc          $flag, .L, .Lgc
  //.L:
  //  phi.i64      $state_ptr_phi, .Lsrc, $state_ptr, .Lgc, $state_ptr_gc
  //  phi.i64      $young_ptr_phi, .Lsrc, $new_ptr, .Lgc, $young_ptr_gc
  // ... use phis  ...
  //
  //.Lgc:
  //  mov.i64      $fn, caml_call_gc
  //  call.caml_gc $state_ptr_gc, $young_ptr_gc, $fn, $state_ptr, $new_ptr, .L
  Block *noGcBlock;
  PhiInst *statePtrPhi;
  PhiInst *youngPtrPhi;
  PhiInst *youngLimitPhi = nullptr;
  PhiInst *exnPtrPhi = nullptr;
  if (!cont) {
    std::vector<Ref<Inst>> phis;

    noGcBlock = new Block(block->getName());
    func->insertAfter(std::next(block->getIterator()), noGcBlock);

    statePtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(statePtrPhi);
    phis.push_back(statePtrPhi);

    youngPtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(youngPtrPhi);
    phis.push_back(youngPtrPhi);

    if (youngLimit) {
      youngLimitPhi = new PhiInst(Type::I64);
      noGcBlock->AddInst(youngLimitPhi);
      phis.push_back(youngLimitPhi);
    }
    if (exnPtr) {
      exnPtrPhi = new PhiInst(Type::I64);
      noGcBlock->AddInst(exnPtrPhi);
      phis.push_back(exnPtrPhi);
    }

    ReturnInst *retInst = new ReturnInst(phis, {});
    noGcBlock->AddInst(retInst);
    assert(call->use_empty() && "tail call has uses");
  } else {
    std::vector<Ref<Inst>> phis;

    if (cont->pred_size() == 1) {
      noGcBlock = cont;
    } else {
      noGcBlock = new Block((block->getName() + "no_gc").str());
      func->insertAfter(std::next(block->getIterator()), noGcBlock);
      JumpInst *jump = new JumpInst(noGcBlock, {});
      noGcBlock->AddInst(jump);
    }
    statePtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(statePtrPhi, &*noGcBlock->begin());
    phis.push_back(statePtrPhi);

    youngPtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(youngPtrPhi, &*noGcBlock->begin());
    phis.push_back(youngPtrPhi);

    if (youngLimit) {
      youngLimitPhi = new PhiInst(Type::I64);
      noGcBlock->AddInst(youngLimitPhi, &*noGcBlock->begin());
      phis.push_back(youngLimitPhi);
    }
    if (exnPtr) {
      exnPtrPhi = new PhiInst(Type::I64);
      noGcBlock->AddInst(exnPtrPhi, &*noGcBlock->begin());
      phis.push_back(exnPtrPhi);
    }

    call->replaceAllUsesWith(phis);
  }
  AnnotSet annot = call->GetAnnots();
  call->eraseFromParent();

  std::vector<Type> callType;
  statePtrPhi->Add(block, statePtr);
  callType.push_back(Type::I64);
  youngPtrPhi->Add(block, youngPtr);
  callType.push_back(Type::I64);
  if (youngLimit) {
    youngLimitPhi->Add(block, youngLimit);
    callType.push_back(Type::I64);
  }
  if (exnPtr) {
    exnPtrPhi->Add(block, exnPtr);
    callType.push_back(Type::I64);
  }

  // Create the GC block.
  Block *gcBlock = new Block((block->getName() + "gc").str());
  {
    func->AddBlock(gcBlock);

    Global *gcFunc = prog->GetGlobalOrExtern("caml_call_gc");

    MovInst *gcName = new MovInst(Type::I64, gcFunc, {});
    gcBlock->AddInst(gcName);
    Inst *gcCall;

    std::vector<Ref<Inst>> gcArgs;
    gcArgs.push_back(statePtr);
    gcArgs.push_back(youngPtr);
    if (youngLimit) {
      gcArgs.push_back(youngLimit);
    }
    if (exnPtr) {
      gcArgs.push_back(exnPtr);
    }

    if (raise) {
      gcCall = new InvokeInst(
          callType,
          gcName,
          gcArgs,
          std::vector<TypeFlag>(gcArgs.size(), TypeFlag::GetNone()),
          noGcBlock,
          raise,
          std::nullopt,
          CallingConv::CAML_GC,
          std::move(annot)
      );
      for (PhiInst &phi : raise->phis()) {
        Ref<Inst> val = phi.GetValue(block);
        phi.Remove(block);
        phi.Add(gcBlock, val);
      }
    } else {
      gcCall = new CallInst(
          callType,
          gcName,
          gcArgs,
          std::vector<TypeFlag>(gcArgs.size(), TypeFlag::GetNone()),
          noGcBlock,
          std::nullopt,
          CallingConv::CAML_GC,
          std::move(annot)
      );
    }
    gcBlock->AddInst(gcCall);
    statePtrPhi->Add(gcBlock, gcCall->GetSubValue(0));
    youngPtrPhi->Add(gcBlock, gcCall->GetSubValue(1));
    if (youngLimit) {
      youngLimitPhi->Add(gcBlock, gcCall->GetSubValue(2));
    }
    if (exnPtr) {
      exnPtrPhi->Add(gcBlock, gcCall->GetSubValue(3));
    }
  }

  // Either use the cached limit or load it from the state.
  Ref<Inst> youngLimitVal;
  if (youngLimitPhi) {
    youngLimitVal = youngLimit;
  } else {
    MovInst *offInst = new MovInst(Type::I64, new ConstantInt(8), {});
    block->AddInst(offInst);
    AddInst *addInst = new AddInst(Type::I64, statePtr, offInst, {});
    block->AddInst(addInst);
    LoadInst *loadInst = new LoadInst(Type::I64, addInst, {});
    block->AddInst(loadInst);
    youngLimitVal = loadInst;
  }

  // Add the comparison dispatching either to the gc or no gc blocks.
  CmpInst *cmpInst = new CmpInst(Type::I8, Cond::UGE, youngPtr, youngLimitVal, {});
  block->AddInst(cmpInst);

  JumpCondInst *jccInst = new JumpCondInst(cmpInst, noGcBlock, gcBlock, {});
  jccInst->SetAnnot<Probability>(1, 1);
  block->AddInst(jccInst);
}

// -----------------------------------------------------------------------------
void CamlAllocInlinerPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    for (auto bt = func.begin(); bt != func.end(); ) {
      auto *term = (bt++)->GetTerminator();
      switch (term->GetKind()) {
        case Inst::Kind::CALL: {
          auto *call = static_cast<CallInst *>(term);
          if (call->GetCallingConv() != CallingConv::CAML_ALLOC) {
            continue;
          }
          InlineCall(call, call->GetCont(), nullptr);
          continue;
        }
        case Inst::Kind::TAIL_CALL: {
          auto *call = static_cast<TailCallInst *>(term);
          if (call->GetCallingConv() != CallingConv::CAML_ALLOC) {
            continue;
          }
          InlineCall(call, nullptr, nullptr);
          continue;
        }
        case Inst::Kind::INVOKE: {
          auto *call = static_cast<InvokeInst *>(term);
          if (call->GetCallingConv() != CallingConv::CAML_ALLOC) {
            continue;
          }
          InlineCall(call, call->GetCont(), call->GetThrow());
          continue;
        }
        case Inst::Kind::RETURN:
        case Inst::Kind::JUMP_COND:
        case Inst::Kind::JUMP:
        case Inst::Kind::SWITCH:
        case Inst::Kind::TRAP:
        case Inst::Kind::RAISE: {
          continue;
        }
        default: {
          llvm_unreachable("not a terminator");
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *CamlAllocInlinerPass::GetPassName() const
{
  return "OCaml allocation inlining";
}
