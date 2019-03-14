// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/clone.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/insts_memory.h"
#include "core/prog.h"
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
static bool HasSingleUse(Func *func)
{
  unsigned numUses = 0;
  for (auto *user : func->users()) {
    if (!user || !user->Is(Value::Kind::INST)) {
      return false;
    }
    auto *inst = static_cast<Inst *>(user);
    for (auto *calls : inst->users()) {
      if (++numUses > 1) {
        return false;
      }
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
static bool HasDataUse(Func *func)
{
  unsigned numUses = 0;
  for (auto *user : func->users()) {
    if (user == nullptr) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
static bool CompatibleCall(CallingConv callee, CallingConv caller)
{
  if (caller != CallingConv::OCAML && callee != CallingConv::OCAML) {
    return true;
  }
  return callee == caller;
}

// -----------------------------------------------------------------------------
class InlineContext final : public CloneVisitor {
public:
  InlineContext(CallInst *call, Func *callee)
    : call_(call)
    , entry_(call->getParent())
    , callee_(callee)
    , caller_(entry_->getParent())
    , stackSize_(caller_->GetStackSize())
    , exit_(nullptr)
    , phi_(nullptr)
    , numExits_(0)
    , isTailCall_(false)
    , rpot_(callee_)
  {
    // Adjust the caller's stack.
    auto *caller = entry_->getParent();
    if (unsigned stackSize = callee->GetStackSize()) {
      caller->SetStackSize(stackSize_ + stackSize);
    }

    // If entry address is taken in callee, split entry.
    if (!callee->getEntryBlock().use_empty()) {
      auto *newEntry = entry_->splitBlock(call_->getIterator());
      entry_->AddInst(new JumpInst(newEntry));
      for (auto *user : entry_->users()) {
        if (auto *phi = ::dyn_cast_or_null<PhiInst>(user)) {
          // TODO: fixup PHI nodes to point to the new entry.
          assert(!"not implemented");
        }
      }
      entry_ = newEntry;
    }

    // Prepare the arguments.
    for (auto *arg : call->args()) {
      args_.push_back(arg);
    }

    // Checks if the function has a single exit.
    unsigned numBlocks = 0;
    for (auto *block : rpot_) {
      numBlocks++;
      if (block->succ_empty()) {
        numExits_++;
      }
    }

    // Split the block if the callee's CFG has multiple blocks.
    if (numBlocks > 1) {
      exit_ = entry_->splitBlock(++call_->getIterator());
    }

    if (numExits_ > 1) {
      if (auto type = call_->GetType()) {
        phi_ = new PhiInst(*type);
        exit_->AddPhi(phi_);
        call_->replaceAllUsesWith(phi_);
      }
      call_->eraseFromParent();
      call_ = nullptr;
    }

    // Handle all reachable blocks.
    auto *after = entry_;
    for (auto &block : rpot_) {
      if (block == &callee->getEntryBlock()) {
        blocks_[block] = entry_;
        continue;
      }
      if (block->succ_empty() && numExits_ <= 1) {
        blocks_[block] = exit_;
        continue;
      }

      // Form a name, containing the callee name, along with
      // the caller name to make it unique.
      std::ostringstream os;
      os << block->GetName();
      os << "$" << caller->GetName();
      os << "$" << callee->GetName();
      auto *newBlock = new Block(os.str());
      caller->insertAfter(after->getIterator(), newBlock);
      after = newBlock;
      blocks_[block] = newBlock;
    }
  }

  InlineContext(TailCallInst *call, Func *callee)
    : call_(nullptr)
    , entry_(call->getParent())
    , callee_(callee)
    , caller_(entry_->getParent())
    , stackSize_(caller_->GetStackSize())
    , exit_(nullptr)
    , phi_(nullptr)
    , numExits_(0)
    , isTailCall_(true)
    , rpot_(callee_)
  {
    // Adjust the caller's stack.
    auto *caller = entry_->getParent();
    if (unsigned stackSize = callee->GetStackSize()) {
      caller->SetStackSize(stackSize_ + stackSize);
    }

    // Prepare the arguments.
    for (auto *arg : call->args()) {
      args_.push_back(arg);
    }

    // Call can be erased - unused from this point onwards.
    call->eraseFromParent();

    // Handle all reachable blocks.
    auto *after = entry_;
    for (auto &block : rpot_) {
      if (block == &callee->getEntryBlock()) {
        blocks_[block] = entry_;
        continue;
      } else {
        // Form a name, containing the callee name, along with
        // the caller name to make it unique.
        std::ostringstream os;
        os << block->GetName();
        os << "$" << caller->GetName();
        os << "$" << callee->GetName();
        auto *newBlock = new Block(os.str());
        caller->insertAfter(after->getIterator(), newBlock);
        after = newBlock;
        blocks_[block] = newBlock;
      }
    }
  }

  /// Inlines the function.
  void Inline()
  {
    // Inline all blocks from the callee.
    for (auto *block : rpot_) {
      auto *target = Map(block);
      Inst *insert;
      if (target->IsEmpty()) {
        insert = nullptr;
      } else {
        if (target == entry_) {
          insert = exit_ ? nullptr : call_;
        } else {
          insert = &*target->begin();
        }
      }

      for (auto &inst : *block) {
        insts_[&inst] = Duplicate(target, insert, &inst);
      }
    }

    // Fix up PHIs.
    for (auto &phi : phis_) {
      auto *phiInst = phi.first;
      auto *phiNew = phi.second;
      for (unsigned i = 0; i < phiInst->GetNumIncoming(); ++i) {
        phiNew->Add(Map(phiInst->GetBlock(i)), CloneVisitor::Map(phiInst->GetValue(i)));
      }
    }
  }

private:
  /// Creates a copy of an instruction.
  Inst *Duplicate(Block *block, Inst *before, Inst *inst)
  {
    auto add = [block, before] (Inst *inst) {
      block->AddInst(inst, before);
      return inst;
    };

    auto ret = [block, this] (Inst *value) {
      if (value) {
        if (phi_) {
          phi_->Add(block, value);
        } else if (call_) {
          call_->replaceAllUsesWith(value);
        }
      }
      if (numExits_ > 1) {
        block->AddInst(new JumpInst(exit_));
      }
      if (call_) {
        call_->eraseFromParent();
        call_ = nullptr;
      }
    };

    switch (inst->GetKind()) {
      // Convert tail calls to calls if caller does not tail.
      case Inst::Kind::TCALL: {
        if (isTailCall_) {
          add(CloneVisitor::Clone(inst));
        } else {
          auto *callInst = static_cast<TailCallInst *>(inst);
          std::vector<Inst *> args;
          for (auto *arg : callInst->args()) {
            args.push_back(Map(arg));
          }
          Inst *callValue = add(new CallInst(
              callInst->GetType(),
              Map(callInst->GetCallee()),
              args,
              callInst->GetNumFixedArgs(),
              callInst->GetCallingConv(),
              callInst->GetAnnotation()
          ));
          if (callInst->GetType()) {
            ret(callValue);
          } else {
            ret(nullptr);
          }
        }
        return nullptr;
      }
      // Convert tail invokes to invokes if caller does not tail.
      case Inst::Kind::TINVOKE: {
        if (isTailCall_) {
          add(CloneVisitor::Clone(inst));
        } else {
          assert(!"not implemented");
        }
        return nullptr;
      }
      // Propagate value if caller does not tail.
      case Inst::Kind::RET: {
        if (isTailCall_) {
          add(CloneVisitor::Clone(inst));
        } else {
          if (auto *val = static_cast<ReturnInst *>(inst)->GetValue()) {
            ret(Map(val));
          } else {
            ret(nullptr);
          }
        }
        return nullptr;
      }
      case Inst::Kind::ARG: {
        auto *argInst = static_cast<ArgInst *>(inst);
        assert(argInst->GetIdx() < args_.size());
        return args_[argInst->GetIdx()];
      }
      case Inst::Kind::FRAME: {
        auto *frameInst = static_cast<FrameInst *>(inst);
        return add(new FrameInst(
            frameInst->GetType(),
            new ConstantInt(frameInst->GetIdx() + stackSize_)
        ));
      }
      case Inst::Kind::PHI: {
        auto *phiInst = static_cast<PhiInst *>(inst);
        auto *phiNew = new PhiInst(phiInst->GetType());
        phis_.emplace_back(phiInst, phiNew);
        return add(phiNew);
      }
      case Inst::Kind::CALL:
      case Inst::Kind::INVOKE: {
        return add(CloneVisitor::Clone(inst));
      }
      default: {
        auto *newInst = add(CloneVisitor::Clone(inst));
        if (newInst->IsTerminator() && before) {
          for (auto it = before->getIterator(); it != block->end(); ) {
            auto *inst = &*it++;
            call_ = call_ == inst ? nullptr : call_;
            phi_ = phi_ == inst ? nullptr : phi_;
            inst->eraseFromParent();
          }
        }
        return newInst;
      }
    }
  }

  /// Maps a block.
  Block *Map(Block *block) override { return blocks_[block]; }
  /// Maps an instruction.
  Inst *Map(Inst *inst) override { return insts_[inst]; }

private:
  /// Call site being inlined.
  CallInst *call_;
  /// Entry block.
  Block *entry_;
  /// Called function.
  Func *callee_;
  /// Caller function.
  Func *caller_;
  /// Size of the caller's stack, to adjust callee offsets.
  unsigned stackSize_;
  /// Exit block.
  Block *exit_;
  /// Final PHI.
  PhiInst *phi_;
  /// Number of exit nodes.
  unsigned numExits_;
  /// Flag indicating if the call is a tail call.
  bool isTailCall_;
  /// Arguments.
  llvm::SmallVector<Inst *, 8> args_;
  /// Mapping from old to new blocks.
  llvm::DenseMap<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
  /// Block order.
  llvm::ReversePostOrderTraversal<Func *> rpot_;
  /// PHI instruction.
  llvm::SmallVector<std::pair<PhiInst *, PhiInst *>, 10> phis_;
};



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
  std::unordered_map<Func *, std::unique_ptr<Node>> nodes_;
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
  if (auto *main = ::dyn_cast_or_null<Func>(prog->GetGlobal("main"))) {
    roots_.push_back(main);
  }

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
      auto it = nodes_.find(edge.Callee);
      dfs(it->second.get());
    }

    for (int i = 0; i < node->Edges.size(); ) {
      if (visitor(node->Edges[i])) {
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
  auto it = nodes_.emplace(func, nullptr);
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
void InlinerPass::Run(Prog *prog)
{
  CallGraph graph(prog);

  graph.InlineEdge([](auto &edge) {
    auto *inst = edge.CallSite;
    auto *caller = inst->getParent()->getParent();
    auto *callee = edge.Callee;

    if (callee->IsNoInline() || callee->IsVarArg()) {
      // Definitely do not inline noinline and vararg calls.
      return false;
    }

    if (!CompatibleCall(caller->GetCallingConv(), callee->GetCallingConv())) {
      // Do not inline incompatible calling convs.
      return false;
    }

    if (HasDataUse(callee)) {
      // Do not inline functions potentially referenced by data.
      return false;
    }

    if (!HasSingleUse(callee)) {
      // Inline short functions, even if they do not have a single use.
      if (callee->size() != 1 || callee->begin()->size() > 5) {
        return false;
      }
    }

    // If possible, inline the function.
    Inst *target = nullptr;
    switch (inst->GetKind()) {
      case Inst::Kind::CALL: {
        auto *callInst = static_cast<CallInst *>(inst);
        target = callInst->GetCallee();
        InlineContext(callInst, callee).Inline();
        break;
      }
      case Inst::Kind::TCALL: {
        auto *callInst = static_cast<TailCallInst *>(inst);
        target = callInst->GetCallee();
        InlineContext(callInst, callee).Inline();
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
