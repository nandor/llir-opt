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
  if (call->arg_size() != 2) {
    return;
  }
  Ref<Inst> statePtr = call->arg(0);
  Ref<Inst> youngPtr = call->arg(1);

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
  if (!cont) {
    noGcBlock = new Block(block->getName());
    func->insertAfter(std::next(block->getIterator()), noGcBlock);
    statePtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(statePtrPhi);
    youngPtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(youngPtrPhi);

    std::vector<Ref<Inst>> phis{ statePtrPhi, youngPtrPhi  };
    ReturnInst *retInst = new ReturnInst(phis, {});
    noGcBlock->AddInst(retInst);
    assert(call->use_empty() && "tail call has uses");
  } else {
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
    youngPtrPhi = new PhiInst(Type::I64);
    noGcBlock->AddInst(youngPtrPhi, &*noGcBlock->begin());

    std::vector<Ref<Inst>> phis{ statePtrPhi, youngPtrPhi };
    call->replaceAllUsesWith(phis);
  }
  AnnotSet annot = call->GetAnnots();
  call->eraseFromParent();

  statePtrPhi->Add(block, statePtr);
  youngPtrPhi->Add(block, youngPtr);

  // Create the GC block.
  Block *gcBlock = new Block((block->getName() + "gc").str());
  {
    func->AddBlock(gcBlock);

    Global *gcFunc = prog->GetGlobalOrExtern("caml_call_gc");

    MovInst *gcName = new MovInst(Type::I64, gcFunc, {});
    gcBlock->AddInst(gcName);
    Inst *gcCall;

    if (raise) {
      gcCall = new InvokeInst(
          std::vector<Type>{ Type::I64, Type::I64 },
          gcName,
          std::vector<Ref<Inst>>{ statePtr, youngPtr },
          noGcBlock,
          raise,
          2,
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
          std::vector<Type>{ Type::I64, Type::I64 },
          gcName,
          std::vector<Ref<Inst>>{ statePtr, youngPtr },
          noGcBlock,
          2,
          CallingConv::CAML_GC,
          std::move(annot)
      );
    }
    gcBlock->AddInst(gcCall);
    statePtrPhi->Add(gcBlock, gcCall->GetSubValue(0));
    youngPtrPhi->Add(gcBlock, gcCall->GetSubValue(1));
  }

  // Add the comparison dispatching either to the gc or no gc blocks.
  MovInst *offInst = new MovInst(Type::I64, new ConstantInt(8), {});
  block->AddInst(offInst);
  AddInst *addInst = new AddInst(Type::I64, statePtr, offInst, {});
  block->AddInst(addInst);
  LoadInst *youngLimit = new LoadInst(Type::I64, addInst, {});
  block->AddInst(youngLimit);
  CmpInst *cmpInst = new CmpInst(Type::I8, Cond::UGE, youngPtr, youngLimit, {});
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
        case Inst::Kind::TCALL: {
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
        case Inst::Kind::RET:
        case Inst::Kind::JCC:
        case Inst::Kind::JMP:
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
