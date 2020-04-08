// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <sstream>
#include <stack>

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
class TrampolineGraph final {
public:
  TrampolineGraph(const Prog *prog)
  {
    BuildGraph(prog);

    for (const Func &f : *prog) {
      auto it = graph_.find(&f);
      if (it != graph_.end() && it->second.Index == 0) {
        Visit(&f);
      }
    }
  }

  bool NeedsTrampoline(const Value *callee)
  {
    if (auto *movInst = ::dyn_cast_or_null<const MovInst>(callee)) {
      auto *movVal = movInst->GetArg();
      switch (movVal->GetKind()) {
        case Value::Kind::INST:
          return true;
        case Value::Kind::GLOBAL:
          switch (static_cast<Global *>(movVal)->GetKind()) {
            case Global::Kind::SYMBOL:
            case Global::Kind::EXTERN:
              return false;
            case Global::Kind::FUNC: {
              auto *func = static_cast<Func *>(movVal);
              switch (func->GetCallingConv()) {
                case CallingConv::C:
                case CallingConv::FAST:
                  return graph_[func].Trampoline;
                case CallingConv::CAML:
                case CallingConv::CAML_ALLOC:
                case CallingConv::CAML_GC:
                case CallingConv::CAML_RAISE:
                  return true;
              }
            }
            case Global::Kind::BLOCK:
            case Global::Kind::ATOM:
              llvm_unreachable("invalid call target");
          }
          llvm_unreachable("invalid global kind");
        case Value::Kind::EXPR:
        case Value::Kind::CONST:
          llvm_unreachable("invalid call target");
      }
      llvm_unreachable("invalid value kind");
    }
    return true;
  }

private:

  void BuildGraph(const Prog *prog)
  {
    for (const Func &func : *prog) {
      for (const Block &block : func) {
        // Start building the graph at C call sites.
        switch (func.GetCallingConv()) {
          case CallingConv::C:
          case CallingConv::FAST:
            break;
          case CallingConv::CAML:
          case CallingConv::CAML_ALLOC:
          case CallingConv::CAML_GC:
          case CallingConv::CAML_RAISE:
            continue;
        }

        // Look for callees - indirect call sites and allocators need trampolines.
        for (const Inst &inst : block) {
          const Value *callee;
          switch (inst.GetKind()) {
            case Inst::Kind::CALL:
              callee = static_cast<const CallSite<Inst> *>(&inst)->GetCallee();
              break;
            case Inst::Kind::TCALL:
            case Inst::Kind::INVOKE:
            case Inst::Kind::TINVOKE:
              callee = static_cast<const CallSite<TerminatorInst> *>(&inst)->GetCallee();
              break;
            case Inst::Kind::JI:
              graph_[&func].Trampoline = true;
              continue;
            default:
              continue;
          }

          if (auto *movInst = ::dyn_cast_or_null<const MovInst>(callee)) {
            auto *movVal = movInst->GetArg();
            switch (movVal->GetKind()) {
              case Value::Kind::INST:
                break;
              case Value::Kind::GLOBAL:
                switch (static_cast<const Global *>(movVal)->GetKind()) {
                  case Global::Kind::SYMBOL:
                  case Global::Kind::EXTERN:
                    break;
                  case Global::Kind::FUNC: {
                    static const char *tramps[] = {
                      "caml_stat_alloc_noexc",
                      "caml_stat_resize_noexc",
                      "caml_raise",
                    };
                    bool needsTrampoline = false;
                    for (const char *tramp : tramps) {
                      if (tramp == func.GetName()) {
                        needsTrampoline = true;
                        break;
                      }
                    }
                    if (needsTrampoline) {
                      graph_[&func].Trampoline = true;
                    } else {
                      graph_[&func].Out.insert(static_cast<const Func *>(movVal));
                    }
                    continue;
                  }
                  case Global::Kind::BLOCK:
                  case Global::Kind::ATOM:
                    llvm_unreachable("invalid call target");
                }
                break;
              case Value::Kind::EXPR:
              case Value::Kind::CONST:
                llvm_unreachable("invalid call target");
            }
          }

          graph_[&func].Trampoline = true;
        }
      }
    }
  }

  void Visit(const Func *func) {
    Node &node = graph_[func];
    node.Index = index_;
    node.LowLink = index_;
    index_++;
    stack_.push(func);
    node.OnStack = true;

    for (const Func *w : node.Out) {
      auto &nodeW = graph_[w];
      if (nodeW.Index == 0) {
        Visit(w);
        node.LowLink = std::min(node.LowLink, nodeW.LowLink);
      } else if (nodeW.OnStack) {
        node.LowLink = std::min(node.LowLink, nodeW.LowLink);
      }
    }

    if (node.LowLink == node.Index) {
      std::vector<const Func *> scc{ func };

      const Func *w;
      do {
        w = stack_.top();
        scc.push_back(w);
        stack_.pop();
        graph_[w].OnStack = false;
      } while (w != func);

      bool needsTrampoline = false;
      for (const Func *w : scc) {
        auto &node = graph_[w];
        if (node.Trampoline) {
          needsTrampoline = true;
          break;
        }
        for (const Func *v : node.Out) {
          if (graph_[v].Trampoline) {
            needsTrampoline = true;
            break;
          }
        }
        if (needsTrampoline) {
          break;
        }
      }

      if (needsTrampoline) {
        for (const Func *w : scc) {
          graph_[w].Trampoline = true;
        }
      }
    }
  }

private:
  /// Graph node.
  struct Node {
    std::set<const Func *> Out;
    unsigned Index = 0;
    unsigned LowLink = 0;
    bool OnStack = false;
    bool Trampoline = false;
  };
  /// Call graph.
  std::unordered_map<const Func *, Node> graph_;

  /// SCC index.
  unsigned index_ = 1;
  /// SCC stack.
  std::stack<const Func *> stack_;
};

// -----------------------------------------------------------------------------
class InlineContext final : public CloneVisitor {
public:
  template<typename T>
  InlineContext(T *call, Func *callee, TrampolineGraph &graph)
    : isTailCall_(call->Is(Inst::Kind::TCALL) || call->Is(Inst::Kind::TINVOKE))
    , isVirtCall_(call->Is(Inst::Kind::INVOKE) || call->Is(Inst::Kind::TINVOKE))
    , type_(call->GetType())
    , call_(isTailCall_ ? nullptr : call)
    , callAnnot_(call->GetAnnot())
    , entry_(call->getParent())
    , callee_(callee)
    , caller_(entry_->getParent())
    , exit_(nullptr)
    , phi_(nullptr)
    , numExits_(0)
    , needsExit_(false)
    , rpot_(callee_)
    , graph_(graph)
  {
    // Prepare the arguments.
    for (auto *arg : call->args()) {
      args_.push_back(arg);
    }

    // Adjust the caller's stack.
    {
      auto *caller = entry_->getParent();
      unsigned maxIndex = 0;
      for (auto &object : caller->objects()) {
        maxIndex = std::max(maxIndex, object.Index);
      }
      for (auto &object : callee->objects()) {
        unsigned newIndex = maxIndex + object.Index + 1;
        frameIndices_.insert({ object.Index, newIndex });
        caller->AddStackObject(newIndex, object.Size, object.Alignment);
      }
    }

    // Exit is needed when C is inlined into OCaml.
    for (Block *block : rpot_) {
      for (Inst &inst : *block) {
        if (auto *mov = ::dyn_cast_or_null<MovInst>(&inst)) {
          if (auto *reg = ::dyn_cast_or_null<ConstantReg>(mov->GetArg())) {
            if (reg->GetValue() == ConstantReg::Kind::RET_ADDR) {
              needsExit_ = true;
            }
          }
        }
      }
    }

    if (isTailCall_) {
      call->eraseFromParent();
    } else {
      // If entry address is taken in callee, split entry.
      if (!callee_->getEntryBlock().use_empty()) {
        auto *newEntry = entry_->splitBlock(call_->getIterator());
        entry_->AddInst(new JumpInst(newEntry, {}));
        for (auto it = entry_->use_begin(); it != entry_->use_end(); ) {
          Use &use = *it++;
          if (auto *phi = ::dyn_cast_or_null<PhiInst>(use.getUser())) {
            use = newEntry;
          }
        }
        entry_ = newEntry;
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
      if (numBlocks > 1 || needsExit_) {
        exit_ = entry_->splitBlock(++call_->getIterator());
      }

      // Create a PHI node if there are multiple exits.
      if (numExits_ > 1) {
        if (type_) {
          phi_ = new PhiInst(*type_);
          if (call_->HasAnnot(CAML_VALUE)) {
            phi_->SetAnnot(CAML_VALUE);
          }
          exit_->AddPhi(phi_);
          call_->replaceAllUsesWith(phi_);
        }
        call_->eraseFromParent();
        call_ = nullptr;
      }
    }

    // Find an equivalent for all blocks in the target function.
    auto *after = entry_;
    for (Block *block : rpot_) {
      if (block == &callee_->getEntryBlock()) {
        blocks_[block] = entry_;
        continue;
      }
      const bool canUseEntry = numExits_ <= 1 && !needsExit_;
      if (block->succ_empty() && !isTailCall_ && canUseEntry) {
        blocks_[block] = exit_;
        continue;
      }


      static int id = 0;
      // Form a name, containing the callee name, along with
      // the caller name to make it unique.
      std::ostringstream os;
      os << block->GetName();
      os << "$" << caller_->GetName();
      os << "$" << callee_->GetName();
      auto *newBlock = new Block(os.str());
      caller_->insertAfter(after->getIterator(), newBlock);
      after = newBlock;
      blocks_[block] = newBlock;
    }
  }

  /// Inlines the function.
  void Inline()
  {
    // Inline all blocks from the callee.
    for (auto *block : rpot_) {
      // Decide which block to place the instruction in.
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
        // Duplicate the instruction, placing it at the desired point.
        insts_[&inst] = Duplicate(target, insert, &inst);
      }
    }

    // Apply PHI fixups.
    Fixup();

    // Remove the call site (can stay there if the function never returns).
    if (call_) {
      if (call_->GetNumRets() > 0) {
        Inst *undef = new UndefInst(call_->GetType(0), call_->GetAnnot());
        entry_->AddInst(undef, call_);
        call_->replaceAllUsesWith(undef);
      }
      call_->eraseFromParent();
    }
  }

private:
  /// Creates a copy of an instruction.
  Inst *Duplicate(Block *block, Inst *&before, Inst *inst)
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
          if (call_->HasAnnot(CAML_VALUE) && !value->HasAnnot(CAML_VALUE)) {
            value->SetAnnot(CAML_VALUE);
          }
          call_->replaceAllUsesWith(value);
        }
      }
      if (numExits_ > 1 || needsExit_) {
        block->AddInst(new JumpInst(exit_, {}));
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
          Inst *callValue = add(new CallInst(
              callInst->GetType(),
              Map(callInst->GetCallee()),
              CloneVisitor::CloneArgs<TailCallInst>(callInst),
              callInst->GetNumFixedArgs(),
              callInst->GetCallingConv(),
              Annot(inst)
          ));

          if (type_) {
            if (auto type = callInst->GetType()) {
              if (type_ == type) {
                ret(callValue);
              } else {
                ret(add(Extend(
                    *type_,
                    *type,
                    callValue,
                    callAnnot_.Without(CAML_FRAME)
                )));
              }
            } else {
              ret(add(new UndefInst(*type_, {})));
            }
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
          llvm_unreachable("not implemented");
        }
        return nullptr;
      }
      // Propagate value if caller does not tail.
      case Inst::Kind::RET: {
        if (isTailCall_) {
          add(CloneVisitor::Clone(inst));
        } else {
          if (type_) {
            if (auto *val = static_cast<ReturnInst *>(inst)->GetValue()) {
              auto *retInst = Map(val);
              auto retType = retInst->GetType(0);
              if (type_ != retType) {
                ret(add(Extend(
                    *type_,
                    retType,
                    retInst,
                    callAnnot_.Without(CAML_FRAME)
                )));
              } else {
                ret(retInst);
              }
            } else {
              ret(add(new UndefInst(*type_, {})));
            }
          } else {
            ret(nullptr);
          }
        }
        return nullptr;
      }
      // Map argument to incoming value.
      case Inst::Kind::ARG: {
        auto *argInst = static_cast<ArgInst *>(inst);
        assert(argInst->GetIdx() < args_.size());
        auto *valInst = args_[argInst->GetIdx()];

        auto argType = argInst->GetType(0);
        auto valType = valInst->GetType(0);
        if (argType != valType) {
          return add(Extend(argType, valType, valInst, Annot(argInst)));
        }
        return valInst;
      }
      // Adjust stack offset.
      case Inst::Kind::FRAME: {
        auto *frameInst = static_cast<FrameInst *>(inst);
        auto it = frameIndices_.find(frameInst->GetObject());
        assert(it != frameIndices_.end() && "frame index out of range");
        return add(new FrameInst(
            frameInst->GetType(),
            new ConstantInt(it->second),
            new ConstantInt(frameInst->GetIndex()),
            Annot(frameInst)
        ));
      }
      // The semantics of mov change.
      case Inst::Kind::MOV: {
        auto *mov = static_cast<MovInst *>(inst);
        if (auto *reg = ::dyn_cast_or_null<ConstantReg>(mov->GetArg())) {
          switch (reg->GetValue()) {
            // Instruction which take the return address of a function.
            case ConstantReg::Kind::RET_ADDR: {
              // If the callee is annotated, add a frame to the jump.
              if (exit_) {
                return add(new MovInst(mov->GetType(), exit_, mov->GetAnnot()));
              } else {
                return add(CloneVisitor::Clone(mov));
              }
            }
            // Instruction which takes the frame address: take SP instead.
            case ConstantReg::Kind::FRAME_ADDR: {
              return add(new MovInst(
                  mov->GetType(),
                  new ConstantReg(ConstantReg::Kind::RSP),
                  mov->GetAnnot()
              ));
            }
            default: {
              return add(CloneVisitor::Clone(mov));
            }
          }
        } else {
          return add(CloneVisitor::Clone(mov));
        }
      }
      // Terminators need to remove all other instructions in target block.
      case Inst::Kind::INVOKE:
      case Inst::Kind::JMP:
      case Inst::Kind::JCC:
      case Inst::Kind::JI:
      case Inst::Kind::TRAP: {
        auto *newInst = add(CloneVisitor::Clone(inst));
        if (before) {
          for (auto it = before->getIterator(); it != block->end(); ) {
            auto *inst = &*it++;
            call_ = call_ == inst ? nullptr : call_;
            phi_ = phi_ == inst ? nullptr : phi_;
            inst->eraseFromParent();
          }
        }
        return newInst;
      }
      // Simple instructions which can be cloned.
      default: {
        return add(CloneVisitor::Clone(inst));
      }
    }
  }

  /// Maps a block.
  Block *Map(Block *block) override { return blocks_[block]; }
  /// Maps an instruction.
  Inst *Map(Inst *inst) override { return insts_[inst]; }

  /// Inlines annotations.
  AnnotSet Annot(const Inst *inst) override
  {
    AnnotSet annots;
    for (const auto &annot : inst->annots()) {
      switch (annot) {
        case CAML_FRAME: {
          annots.Set(callAnnot_.Has(CAML_ROOT) ? CAML_ROOT : CAML_FRAME);
          break;
        }
        case CAML_ADDR:
        case CAML_ROOT:
        case CAML_VALUE: {
          annots.Set(annot);
          break;
        }
      }
    }

    const Value *callee;
    switch (inst->GetKind()) {
      case Inst::Kind::CALL:
        callee = static_cast<const CallSite<Inst> *>(inst)->GetCallee();
        break;
      case Inst::Kind::TCALL:
      case Inst::Kind::INVOKE:
      case Inst::Kind::TINVOKE:
        callee = static_cast<const CallSite<TerminatorInst> *>(inst)->GetCallee();
        break;
      default:
        return annots;
    }

    if (graph_.NeedsTrampoline(callee)) {
      if (callAnnot_.Has(CAML_ROOT)) {
        annots.Set(CAML_ROOT);
      }
      if (callAnnot_.Has(CAML_FRAME)) {
        annots.Set(CAML_FRAME);
      }
    }
    return annots;
  }

  /// Extends a value from one type to another.
  Inst *Extend(Type argType, Type valType, Inst *valInst, AnnotSet annot)
  {
    if (IsIntegerType(argType) && IsIntegerType(valType)) {
      if (GetSize(argType) < GetSize(valType)) {
        // Zero-extend integral argument to match.
        return new TruncInst(argType, valInst, annot);
      } else {
        // Truncate integral argument to match.
        return new ZExtInst(argType, valInst, annot);
      }
    }
    llvm_unreachable("Cannot extend/cast type");
  }

private:
  /// Flag indicating if the call is a tail call.
  const bool isTailCall_;
  /// Flag indicating if the call is a virtual call.
  const bool isVirtCall_;
  /// Return type of the call.
  const std::optional<Type> type_;
  /// Call site being inlined.
  Inst *call_;
  /// Annotations of the original call.
  const AnnotSet callAnnot_;
  /// Entry block.
  Block *entry_;
  /// Called function.
  Func *callee_;
  /// Caller function.
  Func *caller_;
  /// Mapping from callee to caller frame indices.
  llvm::DenseMap<unsigned, unsigned> frameIndices_;
  /// Exit block.
  Block *exit_;
  /// Final PHI.
  PhiInst *phi_;
  /// Number of exit nodes.
  unsigned numExits_;
  /// Flag to indicate if a separate exit label is needed.
  bool needsExit_;
  /// Arguments.
  llvm::SmallVector<Inst *, 8> args_;
  /// Mapping from old to new blocks.
  llvm::DenseMap<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
  /// Block order.
  llvm::ReversePostOrderTraversal<Func *> rpot_;
  /// Graph which determines calls needing trampolines.
  TrampolineGraph graph_;
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
    if (dataUses == 0) {
      // No data uses - heuristic is based solely on code uses.
      if (codeUses > 1) {
        // Inline short functions, even if they do not have a single use.
        if (callee->size() != 1 || callee->begin()->size() > 5) {
          return false;
        }
      }
    } else {
      // Inline functions with a single code use.
      if (codeUses != 1) {
        return false;
      }
    }

    auto *inst = edge.CallSite;
    auto *caller = inst->getParent()->getParent();
    if (callee == caller) {
      // Do not inline tail-recursive calls.
      return false;
    }

    // If possible, inline the function.
    Inst *target = nullptr;
    switch (inst->GetKind()) {
      case Inst::Kind::CALL: {
        auto *callInst = static_cast<CallInst *>(inst);
        target = callInst->GetCallee();
        InlineContext(callInst, callee, tg).Inline();
        break;
      }
      case Inst::Kind::TCALL: {
        auto *callInst = static_cast<TailCallInst *>(inst);
        target = callInst->GetCallee();
        InlineContext(callInst, callee, tg).Inline();
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
