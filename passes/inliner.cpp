// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
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
class InlineContext {
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

    // Checks if the last block terminates with a jump. If that is
    // the case, the entry must be split to avoid double terminators.
    bool jumps = false;
    if (auto *term = callee->rbegin()->GetTerminator()) {
      switch (term->GetKind()) {
        case Inst::Kind::JMP:
        case Inst::Kind::JI:
        case Inst::Kind::JCC: {
          jumps = true;
          break;
        }
        default: {
          jumps = false;
          break;
        }
      }
    }

    // Split the block if the callee's CFG has multiple blocks.
    if (numBlocks > 1 || jumps) {
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
        Clone(target, insert, &inst);
      }
    }

    // Fix up PHIs.
    for (auto &phi : phis_) {
      auto *phiInst = phi.first;
      auto *phiNew = phi.second;
      for (unsigned i = 0; i < phiInst->GetNumIncoming(); ++i) {
        phiNew->Add(Map(phiInst->GetBlock(i)), Map(phiInst->GetValue(i)));
      }
    }

    // Infinite loop in function - erase the call if it's still there.
    if (call_) {
      call_->eraseFromParent();
      call_ = nullptr;
    }
  }

private:
  /// Creates a copy of an instruction and tracks them.
  Inst *Clone(Block *block, Inst *before, Inst *inst)
  {
    if (Inst *dup = Duplicate(block, before, inst)) {
      insts_[inst] = dup;
      return dup;
    } else {
      return nullptr;
    }
  }

  /// Creates a copy of an instruction.
  Inst *Duplicate(Block *block, Inst *before, Inst *inst)
  {
    auto add = [block, before] (Inst *inst) {
      block->AddInst(inst, before);
      return inst;
    };

    switch (inst->GetKind()) {
      case Inst::Kind::CALL: {
        auto *callInst = static_cast<CallInst *>(inst);
        std::vector<Inst *> args;
        for (auto *arg : callInst->args()) {
          args.push_back(Map(arg));
        }
        return add(new CallInst(
            callInst->GetType(),
            Map(callInst->GetCallee()),
            args,
            callInst->GetNumFixedArgs(),
            callInst->GetCallingConv(),
            callInst->GetAnnotation()
        ));
      }
      case Inst::Kind::INVOKE: {
        auto *invokeInst = static_cast<InvokeInst *>(inst);
        std::vector<Inst *> args;
        for (auto *arg : invokeInst->args()) {
          args.push_back(Map(arg));
        }
        return add(new InvokeInst(
            invokeInst->GetType(),
            Map(invokeInst->GetCallee()),
            args,
            Map(invokeInst->GetCont()),
            Map(invokeInst->GetThrow()),
            invokeInst->GetNumFixedArgs(),
            invokeInst->GetCallingConv(),
            invokeInst->GetAnnotation()
        ));
      }
      case Inst::Kind::TCALL: {
        auto *callInst = static_cast<TailCallInst *>(inst);
        std::vector<Inst *> args;
        for (auto *arg : callInst->args()) {
          args.push_back(Map(arg));
        }

        if (isTailCall_) {
          // Keep the call as a tail call, terminating the block.
          add(new TailCallInst(
              callInst->GetType(),
              Map(callInst->GetCallee()),
              args,
              callInst->GetNumFixedArgs(),
              callInst->GetCallingConv(),
              callInst->GetAnnotation()
          ));
          return nullptr;
        } else {
          // Convert the tail call to a call and jump to the landing pad.
          auto *value = add(new CallInst(
              callInst->GetType(),
              Map(callInst->GetCallee()),
              args,
              callInst->GetNumFixedArgs(),
              callInst->GetCallingConv(),
              callInst->GetAnnotation()
          ));

          if (callInst->GetType()) {
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

          return nullptr;
        }
      }
      case Inst::Kind::TINVOKE: {
        assert(!"not implemented");
        return nullptr;
      }
      case Inst::Kind::RET: {
        auto *retInst = static_cast<ReturnInst *>(inst);
        if (isTailCall_) {
          if (auto *val = retInst->GetValue()) {
            add(new ReturnInst(Map(val)));
          } else {
            add(new ReturnInst());
          }
          return nullptr;
        } else {
          // Add the returned value to the PHI if one was generated.
          // If there are multiple returns, add a jump to the target.
          if (auto *val = retInst->GetValue()) {
            if (phi_) {
              phi_->Add(block, Map(val));
            } else if (call_) {
              call_->replaceAllUsesWith(Map(val));
            }
          }
          if (numExits_ > 1) {
            block->AddInst(new JumpInst(exit_));
          }
          if (call_) {
            call_->eraseFromParent();
            call_ = nullptr;
          }
          return nullptr;
        }
      }
      case Inst::Kind::JCC: {
        auto *jccInst = static_cast<JumpCondInst *>(inst);
        return add(new JumpCondInst(
            Map(jccInst->GetCond()),
            Map(jccInst->GetTrueTarget()),
            Map(jccInst->GetFalseTarget())
        ));
      }
      case Inst::Kind::JI: {
        auto *jiInst = static_cast<JumpIndirectInst *>(inst);
        return add(new JumpIndirectInst(Map(jiInst->GetTarget())));
      }
      case Inst::Kind::JMP: {
        auto *jmpInst = static_cast<JumpInst *>(inst);
        return add(new JumpInst(Map(jmpInst->GetTarget())));
      }
      case Inst::Kind::SWITCH: {
        auto *switchInst = static_cast<SwitchInst *>(inst);
        std::vector<Block *> branches;
        for (unsigned i = 0; i < switchInst->getNumSuccessors(); ++i) {
          branches.push_back(Map(switchInst->getSuccessor(i)));
        }
        return add(new SwitchInst(Map(switchInst->GetIdx()), branches));
      }
      case Inst::Kind::TRAP: {
        // Erase everything after the inlined trap instruction.
        auto *trapNew = add(new TrapInst());
        if (call_) {
          block->erase(before->getIterator(), block->end());
          call_ = nullptr;
        }
        return trapNew;
      }
      case Inst::Kind::LD: {
        auto *loadInst = static_cast<LoadInst *>(inst);
        return add(new LoadInst(
            loadInst->GetLoadSize(),
            loadInst->GetType(),
            Map(loadInst->GetAddr())
        ));
      }
      case Inst::Kind::ST: {
        auto *storeInst = static_cast<StoreInst *>(inst);
        return add(new StoreInst(
            storeInst->GetStoreSize(),
            Map(storeInst->GetAddr()),
            Map(storeInst->GetVal())
        ));
      }
      case Inst::Kind::XCHG:{
        auto *xchgInst = static_cast<ExchangeInst *>(inst);
        return add(new ExchangeInst(
            xchgInst->GetType(),
            Map(xchgInst->GetAddr()),
            Map(xchgInst->GetVal())
        ));
      }
      case Inst::Kind::SET: {
        auto *setInst = static_cast<SetInst *>(inst);
        return add(new SetInst(setInst->GetReg(), Map(setInst->GetValue())));
      }
      case Inst::Kind::VASTART: {
        auto *vaInst = static_cast<VAStartInst *>(inst);
        return add(new VAStartInst(Map(vaInst->GetVAList())));
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
      case Inst::Kind::SELECT: {
        auto *selectInst = static_cast<SelectInst *>(inst);
        return add(new SelectInst(
            selectInst->GetType(),
            Map(selectInst->GetCond()),
            Map(selectInst->GetTrue()),
            Map(selectInst->GetFalse())
        ));
      }
      case Inst::Kind::MOV: {
        auto *movInst = static_cast<MovInst *>(inst);
        return add(new MovInst(
            movInst->GetType(),
            Map(movInst->GetArg())
        ));
      }
      case Inst::Kind::CMP: {
        auto *cmpInst = static_cast<CmpInst *>(inst);
        return add(new CmpInst(
            cmpInst->GetType(),
            cmpInst->GetCC(),
            Map(cmpInst->GetLHS()),
            Map(cmpInst->GetRHS())
        ));
      }
      case Inst::Kind::ABS:
      case Inst::Kind::NEG:
      case Inst::Kind::SQRT:
      case Inst::Kind::SIN:
      case Inst::Kind::COS:
      case Inst::Kind::SEXT:
      case Inst::Kind::ZEXT:
      case Inst::Kind::FEXT:
      case Inst::Kind::TRUNC: {
        auto *unaryInst = static_cast<UnaryInst *>(inst);
        return add(new UnaryInst(
            unaryInst->GetKind(),
            unaryInst->GetType(),
            Map(unaryInst->GetArg())
        ));
      }

      case Inst::Kind::ADD:
      case Inst::Kind::AND:
      case Inst::Kind::DIV:
      case Inst::Kind::REM:
      case Inst::Kind::MUL:
      case Inst::Kind::OR:
      case Inst::Kind::ROTL:
      case Inst::Kind::SLL:
      case Inst::Kind::SRA:
      case Inst::Kind::SRL:
      case Inst::Kind::SUB:
      case Inst::Kind::XOR:
      case Inst::Kind::POW:
      case Inst::Kind::COPYSIGN: {
        auto *binInst = static_cast<BinaryInst *>(inst);
        return add(new BinaryInst(
            binInst->GetKind(),
            binInst->GetType(),
            Map(binInst->GetLHS()),
            Map(binInst->GetRHS())
        ));
      }
      case Inst::Kind::UADDO:
      case Inst::Kind::UMULO: {
        auto *ovInst = static_cast<OverflowInst *>(inst);
        return add(new OverflowInst(
            ovInst->GetKind(),
            Map(ovInst->GetLHS()),
            Map(ovInst->GetRHS())
        ));
      }
      case Inst::Kind::UNDEF: {
        auto *undefInst = static_cast<UndefInst *>(inst);
        return add(new UndefInst(undefInst->GetType()));
      }
      case Inst::Kind::PHI: {
        auto *phiInst = static_cast<PhiInst *>(inst);
        auto *phiNew = new PhiInst(phiInst->GetType());
        phis_.emplace_back(phiInst, phiNew);
        return add(phiNew);
      }
    }
  }

  /// Maps a value.
  Value *Map(Value *val)
  {
    switch (val->GetKind()) {
      case Value::Kind::INST: {
        return Map(static_cast<Inst *>(val));
      }
      case Value::Kind::GLOBAL: {
        switch (static_cast<Global *>(val)->GetKind()) {
          case Global::Kind::SYMBOL: return val;
          case Global::Kind::EXTERN: return val;
          case Global::Kind::FUNC: return val;
          case Global::Kind::BLOCK: return Map(static_cast<Block *>(val));
          case Global::Kind::ATOM: return val;
        }
      }
      case Value::Kind::EXPR: {
        return val;
      }
      case Value::Kind::CONST: {
        return val;
      }
    }
  }

  /// Maps a block.
  Block *Map(Block *block)
  {
    return blocks_[block];
  }

  /// Maps an instruction.
  Inst *Map(Inst *inst)
  {
    return insts_[inst];
  }

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
  /// Mapping from old to new instructions.
  llvm::DenseMap<Inst *, Inst *> insts_;
  /// Mapping from old to new blocks.
  llvm::DenseMap<Block *, Block *> blocks_;
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
