// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/libc_simplify.h"



// -----------------------------------------------------------------------------
const char *LibCSimplifyPass::kPassID = "libc-simplify";

// -----------------------------------------------------------------------------
const char *LibCSimplifyPass::GetPassName() const
{
  return "libc call simplification";
}


// -----------------------------------------------------------------------------
bool LibCSimplifyPass::Run(Prog &prog)
{
  bool changed = false;
  if (auto *g = prog.GetGlobal("strlen")) {
    Simplify(g, &LibCSimplifyPass::SimplifyFree);
  }
  if (auto *g = prog.GetGlobal("free")) {
    Simplify(g, &LibCSimplifyPass::SimplifyFree);
  }
  return changed;
}

// -----------------------------------------------------------------------------
void LibCSimplifyPass::Simplify(
    Global *g,
    std::optional<Inst *> (LibCSimplifyPass::*f)(CallSite &))
{
  for (User *user : g->users()) {
    auto *mov = ::cast_or_null<MovInst>(user);
    if (!mov) {
      continue;
    }
    for (auto ut = mov->user_begin(); ut != mov->user_end(); ) {
      auto *call = ::cast_or_null<CallSite>(*ut++);
      if (!call || call->GetCallee() != mov->GetSubValue(0)) {
        continue;
      }
      if (auto value = (this->*f)(*call)) {
        switch (call->GetKind()) {
          default: llvm_unreachable("not a call");
          case Inst::Kind::CALL: {
            auto *cont = static_cast<CallInst *>(call)->GetCont();
            call->getParent()->AddInst(new JumpInst(cont, {}), call);
            call->eraseFromParent();
            break;
          }
          case Inst::Kind::TAIL_CALL: {
            llvm_unreachable("not implemented");
          }
          case Inst::Kind::INVOKE: {
            llvm_unreachable("not implemented");
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::optional<Inst *> LibCSimplifyPass::SimplifyFree(CallSite &call)
{
  if (call.arg_size() != 1 || call.type_size() != 0) {
    return std::nullopt;
  }
  auto mov = ::cast_or_null<MovInst>(call.arg(0));
  if (!mov) {
    return std::nullopt;
  }
  auto arg = ::cast_or_null<ConstantInt>(mov->GetArg());
  if (!arg || !arg->GetValue().isNullValue()) {
    return std::nullopt;
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
static std::optional<APInt> EvaluateStrlen(CallSite &call)
{
  if (call.arg_size() != 1 || call.type_size() != 1) {
    return std::nullopt;
  }
  auto mov = ::cast_or_null<MovInst>(call.arg(0));
  if (!mov) {
    return std::nullopt;
  }
  auto atom = ::cast_or_null<Atom>(mov->GetArg());
  if (!atom || atom->empty() || !atom->getParent()->getParent()->IsConstant()) {
    return std::nullopt;
  }
  switch (atom->begin()->GetKind()) {
    case Item::Kind::STRING: {
      auto it = std::next(atom->begin());
      if (it == atom->end()) {
        return std::nullopt;
      }
      if (it->GetKind() != Item::Kind::INT8 || it->GetInt8()) {
        return std::nullopt;
      }
      llvm::errs() << call.getParent()->getName() << "\n";
      return APInt(64, atom->begin()->GetString().size(), true);
    }
    default: {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
std::optional<Inst *> LibCSimplifyPass::SimplifyStrlen(CallSite &call)
{
  auto v = EvaluateStrlen(call);
  if (!v) {
    return std::nullopt;
  }
  auto *mov = new MovInst(call.type(0), new ConstantInt(*v), call.GetAnnots());
  auto *block = call.getParent();
  block->AddInst(mov, &call);
  call.replaceAllUsesWith(mov);
  return mov;
}
