// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <stack>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/insts/binary.h"
#include "core/insts/call.h"
#include "core/insts/control.h"
#include "core/insts/memory.h"
#include "core/prog.h"
#include "passes/inliner/inline_helper.h"
#include "passes/inliner/trampoline_graph.h"
#include "passes/inliner.h"



// -----------------------------------------------------------------------------
static std::pair<unsigned, unsigned> CountUses(Func *func)
{
  unsigned dataUses = 0, codeUses = 0;
  for (const User *user : func->users()) {
    if (auto *inst = ::cast_or_null<const Inst>(user)) {
      if (auto *movInst = ::cast_or_null<const MovInst>(inst)) {
        for (const User *movUsers : movInst->users()) {
          codeUses++;
        }
      } else {
        codeUses++;
      }
    } else {
      dataUses++;
    }
  }
  return { dataUses, codeUses };
}

// -----------------------------------------------------------------------------
static bool CheckGlobalCost(Func *callee)
{
  // Do not inline functions which are too large.
  if (callee->size() > 100) {
    return false;
  }
  auto [dataUses, codeUses] = CountUses(callee);
  // Allow inlining regardless the number of data uses.
  if (codeUses > 1 || dataUses != 0) {
    // Inline short functions, even if they do not have a single use.
    if (callee->size() != 1 || callee->begin()->size() > 10) {
      // Decide based on the number of new instructions.
      unsigned numCopies = (dataUses ? 1 : 0) + codeUses;
      if (numCopies * callee->inst_size() > 20) {
        return false;
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
static bool HasNonLocalBlocks(Func *callee)
{
  for (Block &block : *callee) {
    if (!block.IsLocal()) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
static bool HasAlloca(Func *callee)
{
  for (Block &block : *callee) {
    for (Inst &inst : block) {
      if (inst.Is(Inst::Kind::ALLOCA)) {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
static bool IsInlineCandidate(Func *callee)
{
  if (callee->getName().substr(0, 5) == "caml_") {
    return false;
  }
  if (callee->inst_size() > 100) {
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
static bool HigherOrderCall(CallInst *call, Func *caller, Func *callee)
{
  if (caller->GetCallingConv() != CallingConv::CAML || caller->inst_size() > 1000) {
    return false;
  }
  if (!IsInlineCandidate(callee)) {
    return false;
  }
  bool HasAllocArg = false;
  for (Ref<Inst> arg : call->args()) {
    if (Ref<CallSite> argCallRef = ::cast_or_null<CallSite>(arg)) {
      CallSite *argCall = argCallRef.Get();
      if (Ref<MovInst> movRef = ::cast_or_null<MovInst>(argCall->GetCallee())) {
        if (Ref<Global> g = ::cast_or_null<Global>(movRef->GetArg())) {
          if (g->getName().substr(0, 10) != "caml_alloc") {
            continue;
          }
          HasAllocArg = true;
          break;
        }
      }
    }
  }
  return HasAllocArg;
}

// -----------------------------------------------------------------------------
static bool DestructuredReturn(CallInst *call, Func *caller, Func *callee)
{
  if (caller->GetCallingConv() != CallingConv::CAML || caller->inst_size() > 1000) {
    return false;
  }
  if (!IsInlineCandidate(callee)) {
    return false;
  }

  unsigned loadUses = 0;
  unsigned anyUses = 0;
  for (User *user : call->users()) {
    anyUses++;
    if (auto *load = ::cast_or_null<LoadInst>(user)) {
      loadUses++;
    }
  }
  return loadUses > 0 && anyUses > 2;
}

// -----------------------------------------------------------------------------
class InlineGraph final {
public:
  /// Constructs the call graph.
  InlineGraph(Prog *prog);
  /// Destroys the call graph.
  ~InlineGraph() {}

  /// Traverses all edges which should be inlined.
  void InlineEdge(std::function<bool(Func *, Func *, Inst *)> visitor);

private:
  /// Set of potential root functions which were not visited yet.
  std::vector<Func *> roots_;
};

// -----------------------------------------------------------------------------
InlineGraph::InlineGraph(Prog *prog)
{
  // Find all functions which have external visibility.
  for (auto &func : *prog) {
    if (func.IsRoot()) {
      roots_.push_back(&func);
    }
  }

  // Find all functions which might be invoked indirectly: These are the
  // functions whose address is taken, i.e. used outside a move used by calls.
  for (auto &func : *prog) {
    bool hasAddressTaken = false;
    for (auto *funcUser : func.users()) {
      if (auto *movInst = ::cast_or_null<MovInst>(funcUser)) {
        for (auto *movUser : movInst->users()) {
          if (auto *inst = ::cast_or_null<CallSite>(movUser)) {
            if (inst->GetCallee().Get() == movInst) {
              continue;
            }
          }
          hasAddressTaken = true;
          break;
        }
        if (!hasAddressTaken) {
          continue;
        }
      }
      hasAddressTaken = true;
      break;
    }
    if (hasAddressTaken) {
      roots_.push_back(&func);
    }
  }
}

// -----------------------------------------------------------------------------
void InlineGraph::InlineEdge(std::function<bool(Func *, Func *, Inst *)> visitor)
{
  std::unordered_set<Func *> visited_;

  std::function<void(Func *)> dfs = [&, this](Func *caller) {
    if (!visited_.insert(caller).second) {
      return;
    }

    // Deal with callees first.
    std::vector<std::pair<Inst *, Func *>> sites;
    for (auto &block : *caller) {
      for (auto &inst : block) {
        if (auto *callee = GetCallee(&inst)) {
          sites.emplace_back(&inst, callee);
          dfs(callee);
        }
      }
    }

    bool changed = false;
    for (auto site : sites) {
      Inst *call = site.first;
      Func *callee = site.second;
      ([&] {
        for (auto &block : *caller) {
          for (auto &inst : block) {
            if (&inst == call) {
              if (visitor(caller, callee, call)) {
                if (callee->use_empty() && !callee->IsRoot()) {
                  callee->eraseFromParent();
                }
                changed = true;
                caller->RemoveUnreachable();
              }
              return;
            }
          }
        }
      })();
    }
  };

  while (!roots_.empty()) {
    Func *node = roots_.back();
    roots_.pop_back();
    dfs(node);
  }
}


// -----------------------------------------------------------------------------
const char *InlinerPass::kPassID = "inliner";

// -----------------------------------------------------------------------------
void InlinerPass::Run(Prog *prog)
{
  InlineGraph graph(prog);
  TrampolineGraph tg(prog);

  graph.InlineEdge([&tg](Func *caller, Func *callee, Inst *inst) {
    // Do not inline certain functions.
    switch (callee->GetCallingConv()) {
      case CallingConv::C:
        break;
      case CallingConv::CAML:
        break;
      case CallingConv::CAML_GC:
      case CallingConv::CAML_ALLOC:
      case CallingConv::SETJMP:
        return false;
    }

    if (callee == caller || callee->IsNoInline() || callee->IsVarArg()) {
      // Definitely do not inline recursive, noinline and vararg calls.
      return false;
    }
    if (HasNonLocalBlocks(callee)) {
      // Do not inline the function if unique copies of the blocks are needed.
      return false;
    }
    if (HasAlloca(callee) && caller->GetCallingConv() == CallingConv::CAML) {
      // Do not inline alloca into OCaml callees.
      return false;
    }

    // If possible, inline the function.
    Inst *target = nullptr;
    switch (inst->GetKind()) {
      case Inst::Kind::CALL: {
        auto *callInst = static_cast<CallInst *>(inst);

        if (!CheckGlobalCost(callee)) {
          // Ban functions that reference themselves.
          for (const User *user : callee->users()) {
            if (auto *inst = ::cast_or_null<const Inst>(user)) {
              if (inst->getParent()->getParent() == callee) {
                return false;
              }
            }
          }

          // Even though the callee exceeds a cost, it might be a short
          // higher-order function which might destruct an argument which
          // was allocated juts for the scope of this function.
          bool HOC = HigherOrderCall(callInst, caller, callee);
          bool DSR = DestructuredReturn(callInst, caller, callee);
          if (!HOC && !DSR) {
            return false;
          }
        }
        target = callInst->GetCallee().Get();
        InlineHelper(callInst, callee, tg).Inline();
        break;
      }
      case Inst::Kind::TCALL: {
        if (!CheckGlobalCost(callee)) {
          return false;
        }
        auto *callInst = static_cast<TailCallInst *>(inst);
        target = callInst->GetCallee().Get();
        InlineHelper(callInst, callee, tg).Inline();
        break;
      }
      default: {
        return false;
      }
    }

    if (auto *inst = ::cast_or_null<MovInst>(target)) {
      if (inst->use_empty()) {
        inst->eraseFromParent();
      }
    }
    return true;
  });
}

// -----------------------------------------------------------------------------
const char *InlinerPass::GetPassName() const
{
  return "Inliner";
}
