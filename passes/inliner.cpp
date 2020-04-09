// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <stack>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/insts_memory.h"
#include "core/prog.h"
#include "passes/inliner/inline_helper.h"
#include "passes/inliner/trampoline_graph.h"
#include "passes/inliner.h"



// -----------------------------------------------------------------------------
static bool IsIndirectCall(Inst *inst)
{
  Inst *callee = nullptr;
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      callee = static_cast<CallInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::INVOKE: {
      callee = static_cast<InvokeInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::TCALL: {
      callee = static_cast<TailCallInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::TINVOKE: {
      callee = static_cast<TailInvokeInst *>(inst)->GetCallee();
      break;
    }
    default: {
      return false;
    }
  }

  if (auto *movInst = ::dyn_cast_or_null<MovInst>(callee)) {
    if (auto *global = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
      return false;
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
static Func *GetCallee(Inst *inst)
{
  Inst *callee = nullptr;
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      callee = static_cast<CallInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::INVOKE: {
      callee = static_cast<InvokeInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::TCALL: {
      callee = static_cast<TailCallInst *>(inst)->GetCallee();
      break;
    }
    case Inst::Kind::TINVOKE: {
      callee = static_cast<TailInvokeInst *>(inst)->GetCallee();
      break;
    }
    default: {
      return nullptr;
    }
  }

  if (auto *movInst = ::dyn_cast_or_null<MovInst>(callee)) {
    return ::dyn_cast_or_null<Func>(movInst->GetArg());
  } else {
    return nullptr;
  }
}

// -----------------------------------------------------------------------------
static bool IsCall(const Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TCALL:
    case Inst::Kind::TINVOKE:
      return true;
    default:
      return false;
  }
}

// -----------------------------------------------------------------------------
static std::pair<unsigned, unsigned> CountUses(Func *func)
{
  unsigned dataUses = 0, codeUses = 0;
  for (const User *user : func->users()) {
    if (!user) {
      dataUses++;
    } else {
      if (auto *movInst = ::dyn_cast_or_null<const MovInst>(user)) {
        for (const User *movUsers : movInst->users()) {
          codeUses++;
        }
      } else {
        codeUses++;
      }
    }
  }
  return { dataUses, codeUses };
}

// -----------------------------------------------------------------------------
class CallGraph final {
public:
  /// An edge in the call graph: call site and callee.
  struct Edge {
    /// Site of the call.
    Inst *CallSite;
    /// Callee.
    Func *Callee;

    Edge(Inst *callSite, Func *callee)
      : CallSite(callSite)
      , Callee(callee)
    {
    }
  };

  /// A node in the call graphs.
  struct Node {
    /// Function.
    Func *Function;
    /// Flag indicating if the node has indirect calls.
    bool Indirect;
    /// Outgoing edges.
    std::vector<Edge> Edges;
  };

  /// Constructs the call graph.
  CallGraph(Prog *prog);

  /// Traverses all edges which should be inlined.
  void InlineEdge(std::function<bool(Edge &edge)> visitor);

private:
  /// Explores the call graph, building a set of constraints.
  void Explore(Func *func);

private:
  /// Set of potential root functions which were not visited yet.
  std::vector<Func *> roots_;
  /// Set of explored functions.
  std::unordered_map<unsigned, std::unique_ptr<Node>> nodes_;
};

// -----------------------------------------------------------------------------
CallGraph::CallGraph(Prog *prog)
{
  // Find all functions which have external visibility.
  for (auto &func : *prog) {
    if (func.GetVisibility() == Visibility::EXTERN) {
      roots_.push_back(&func);
    }
  }

  // Find all functions which might be invoked indirectly: These are the
  // functions whose address is taken, i.e. used outside a move used by calls.
  for (auto &func : *prog) {
    bool hasAddressTaken = false;
    for (auto *funcUser : func.users()) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(funcUser)) {
        for (auto *movUser : movInst->users()) {
          if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
            if (IsCall(inst) && inst->Op<0>() == movInst) {
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

  // If available, start the search from main.
  while (!roots_.empty()) {
    Func *node = roots_.back();
    roots_.pop_back();
    Explore(node);
  }
}

// -----------------------------------------------------------------------------
void CallGraph::InlineEdge(std::function<bool(Edge &edge)> visitor)
{
  std::unordered_set<Func *> visited_;

  std::function<void(Node *)> dfs = [&, this](Node *node) {
    if (!visited_.insert(node->Function).second) {
      return;
    }

    for (auto &edge : node->Edges) {
      auto it = nodes_.find(edge.Callee->GetID());
      dfs(it->second.get());
    }

    for (int i = 0; i < node->Edges.size(); ) {
      if (visitor(node->Edges[i])) {
        Func *callee = node->Edges[i].Callee;
        if (callee->use_empty()) {
          callee->eraseFromParent();
        }
        node->Edges[i] = node->Edges.back();
        node->Edges.pop_back();
      } else {
        ++i;
      }
    }
  };

  for (auto &node : nodes_) {
    dfs(node.second.get());
  }
}

// -----------------------------------------------------------------------------
void CallGraph::Explore(Func *func)
{
  auto it = nodes_.emplace(func->GetID(), nullptr);
  if (!it.second) {
    return;
  }
  it.first->second = std::make_unique<Node>();
  auto *node = it.first->second.get();
  node->Function = func;
  node->Indirect = false;

  for (auto &block : *func) {
    for (auto &inst : block) {
      if (auto *callee = GetCallee(&inst)) {
        node->Edges.emplace_back(&inst, callee);
      }
      if (IsIndirectCall(&inst)) {
        node->Indirect = true;
      }
    }
  }

  for (auto &callee : node->Edges) {
    Explore(callee.Callee);
  }
}


// -----------------------------------------------------------------------------
const char *InlinerPass::kPassID = "inliner";

// -----------------------------------------------------------------------------
void InlinerPass::Run(Prog *prog)
{
  CallGraph graph(prog);
  TrampolineGraph tg(prog);

  graph.InlineEdge([&tg](auto &edge) {
    auto *callee = edge.Callee;

    // Do not inline certain functions.
    switch (callee->GetCallingConv()) {
      case CallingConv::FAST:
      case CallingConv::C:
        break;
      case CallingConv::CAML:
        break;
      case CallingConv::CAML_RAISE:
      case CallingConv::CAML_GC:
      case CallingConv::CAML_ALLOC:
        return false;
    }

    if (callee->IsNoInline() || callee->IsVarArg()) {
      // Definitely do not inline noinline and vararg calls.
      return false;
    }

    auto [dataUses, codeUses] = CountUses(callee);

    // Allow inlining regardless the number of data uses.
    if (codeUses > 1 || dataUses != 0) {
      // Inline short functions, even if they do not have a single use.
      if (callee->size() != 1 || callee->begin()->size() > 10) {
        // Decide based on the number of new instructions.
        unsigned numCopies = (dataUses ? 1 : 0) + codeUses;
        unsigned numInsts = 0;
        for (const Block &block : *callee) {
          numInsts += block.size();
        }
        if (numCopies * numInsts > 200) {
          return false;
        }
      }
    }

    auto *inst = edge.CallSite;
    auto *caller = inst->getParent()->getParent();
    if (callee == caller) {
      // Do not inline recursive calls.
      return false;
    }

    // If possible, inline the function.
    Inst *target = nullptr;
    switch (inst->GetKind()) {
      case Inst::Kind::CALL: {
        auto *callInst = static_cast<CallInst *>(inst);
        target = callInst->GetCallee();
        InlineHelper(callInst, callee, tg).Inline();
        break;
      }
      case Inst::Kind::TCALL: {
        auto *callInst = static_cast<TailCallInst *>(inst);
        target = callInst->GetCallee();
        InlineHelper(callInst, callee, tg).Inline();
        return true;
      }
      default: {
        return false;
      }
    }

    if (auto *inst = ::dyn_cast_or_null<MovInst>(target)) {
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
