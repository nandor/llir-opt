// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/call_graph.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/inst_visitor.h"
#include "core/insts.h"
#include "core/object.h"
#include "core/prog.h"
#include "passes/pre_eval/single_execution.h"
#include "passes/pre_eval/tainted_objects.h"



// -----------------------------------------------------------------------------
using BlockInfo = TaintedObjects::BlockInfo;

// -----------------------------------------------------------------------------
static bool AlwaysCalled(const Inst *inst)
{
  for (const User *user : inst->users()) {
    auto *userValue = static_cast<const Value *>(user);
    if (auto *userInst = ::dyn_cast_or_null<const Inst>(userValue)) {
      switch (userInst->GetKind()) {
        case Inst::Kind::CALL: {
          auto &site = static_cast<const CallInst &>(*userInst);
          if (site.GetCallee() != inst) {
            return false;
          }
          continue;
        }
        case Inst::Kind::TCALL:
        case Inst::Kind::INVOKE:
        case Inst::Kind::TINVOKE: {
          auto &site = static_cast<const CallSite<TerminatorInst> &>(*userInst);
          if (site.GetCallee() != inst) {
            return false;
          }
          continue;
        }
        default: {
          return false;
        }
      }
    } else {
      return false;
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
const std::unordered_map<std::string, bool> kCallbacks =
{
  #define SYSCALL(name, callback) { #name, callback },
  #include "core/syscalls.h"
  #undef SYSCALL
};

// -----------------------------------------------------------------------------
bool TaintedObjects::Tainted::Union(const Tainted &that)
{
  bool changed = false;
  changed |= objects_.Union(that.objects_);
  changed |= funcs_.Union(that.funcs_);
  changed |= blocks_.Union(that.blocks_);
  return changed;
}

// -----------------------------------------------------------------------------
bool TaintedObjects::Tainted::Add(ID<Object> object)
{
  return objects_.Insert(object);
}

// -----------------------------------------------------------------------------
bool TaintedObjects::Tainted::Add(ID<Func> func)
{
  return funcs_.Insert(func);
}

// -----------------------------------------------------------------------------
bool TaintedObjects::Tainted::Add(ID<Block> block)
{
  return blocks_.Insert(block);
}

// -----------------------------------------------------------------------------
class BlockBuilder final : public InstVisitor {
public:
  BlockBuilder(
      TaintedObjects *objs,
      ID<BlockInfo> id,
      BlockInfo *&info,
      const TaintedObjects::CallString &cs)
    : objs_(objs)
    , id_(id)
    , info_(info)
    , cs_(cs)
  {
  }

  void VisitCall(CallInst *i) override
  {
    auto next = MapInst(&*std::next(i->getIterator()));
    VisitCall(*info_, *i, { next });
    info_ = objs_->blocks_.Map(next);
  }

  void VisitTailCall(TailCallInst *i) override
  {
    VisitCall(*info_, *i, {
        Exit(i->getParent()->getParent())
    });
  }

  void VisitInvoke(InvokeInst *i) override
  {
    VisitCall(*info_, *i, {
        MapBlock(i->GetCont()),
        MapBlock(i->GetThrow())
    });
  }

  void VisitTailInvoke(TailInvokeInst *i) override
  {
    VisitCall(*info_, *i, {
        MapBlock(i->GetThrow()),
        Exit(i->getParent()->getParent())
    });
  }

  void VisitReturn(ReturnInst *i) override
  {
    info_->Successors.Insert(Exit(i->getParent()->getParent()));
  }

  void VisitJumpCond(JumpCondInst *i) override
  {
    info_->Successors.Insert(MapBlock(i->GetTrueTarget()));
    info_->Successors.Insert(MapBlock(i->GetFalseTarget()));
  }

  void VisitJumpIndirect(JumpIndirectInst *i) override
  {
    objs_->indirectJumps_.emplace_back(cs_, id_);
  }

  void VisitJump(JumpInst *i) override
  {
    info_->Successors.Insert(MapBlock(i->GetTarget()));
  }

  void VisitSwitch(SwitchInst *sw) override
  {
    for (unsigned i = 0, n = sw->getNumSuccessors(); i < n; ++i) {
      info_->Successors.Insert(MapBlock(sw->getSuccessor(i)));
    }
  }

  void VisitTrap(TrapInst *i) override
  {
  }

  void VisitMov(MovInst *inst) override
  {
    auto *arg = static_cast<const MovInst *>(inst)->GetArg();
    auto &taint = info_->Taint;
    switch (arg->GetKind()) {
      case Value::Kind::CONST:
      case Value::Kind::INST: {
        return;
      }
      case Value::Kind::GLOBAL: {
        switch (static_cast<Global *>(arg)->GetKind()) {
          case Global::Kind::EXTERN: {
            return;
          }
          case Global::Kind::BLOCK: {
            taint.Add(objs_->blockMap_.Map(static_cast<Block *>(arg)));
            objs_->queue_.Push(info_->BlockID);
            return;
          }
          case Global::Kind::FUNC: {
            if (!AlwaysCalled(inst)) {
              taint.Add(objs_->funcMap_.Map(static_cast<Func *>(arg)));
              objs_->queue_.Push(info_->BlockID);
            }
            return;
          }
          case Global::Kind::ATOM: {
            auto *obj = static_cast<Atom *>(arg)->getParent();
            taint.Add(objs_->objectMap_.Map(obj));
            objs_->queue_.Push(info_->BlockID);
            return;
          }
        }
        llvm_unreachable("invalid global kind");
      }
      case Value::Kind::EXPR: {
        switch (static_cast<Expr *>(arg)->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto *sym = static_cast<SymbolOffsetExpr *>(arg)->GetSymbol();
            switch (sym->GetKind()) {
              case Global::Kind::EXTERN:
              case Global::Kind::BLOCK:
              case Global::Kind::FUNC: {
                // Pointers into functions are UB.
                return;
              }
              case Global::Kind::ATOM: {
                auto *obj = static_cast<Atom *>(sym)->getParent();
                taint.Add(objs_->objectMap_.Map(obj));
                objs_->queue_.Push(info_->BlockID);
                return;
              }
            }
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid expression kind");
      }
    }
    llvm_unreachable("invalid value kind");
  }

  void Visit(Inst *inst) override {}

private:
  template<typename T>
  void VisitCall(
      BlockInfo &info,
      const CallSite<T> &call,
      std::set<ID<BlockInfo>> &&conts)
  {
    if (auto *mov = ::dyn_cast_or_null<const MovInst>(call.GetCallee())) {
      auto *callee = mov->GetArg();
      switch (callee->GetKind()) {
        case Value::Kind::INST: {
          objs_->indirectCalls_.emplace_back(cs_, id_, std::move(conts));
          return;
        }
        case Value::Kind::GLOBAL: {
          switch (static_cast<Global *>(callee)->GetKind()) {
            case Global::Kind::EXTERN: {
              auto *ext = static_cast<Extern *>(callee);
              std::string name(ext->GetName());
              auto it = kCallbacks.find(name);
              if (it == kCallbacks.end() || it->second) {
                llvm_unreachable("not implemented");
              } else {
                for (auto cont : conts) {
                  objs_->blocks_.Map(id_)->Successors.Insert(cont);
                }
                return;
              }
            }
            case Global::Kind::FUNC: {
              Func &func = *static_cast<Func *>(callee);
              objs_->explore_.emplace(cs_, &func, &info, std::move(conts));
              return;
            }
            case Global::Kind::BLOCK:
            case Global::Kind::ATOM: {
              // Undefined behaviour - no flow.
              return;
            }
          }
          llvm_unreachable("invalid global kind");
        }
        case Value::Kind::CONST:
        case Value::Kind::EXPR: {
          // Undefined behaviour - no flow.
          return;
        }
      }
      llvm_unreachable("invalid value kind");
    } else {
      objs_->indirectCalls_.emplace_back(cs_, id_, std::move(conts));
      return;
    }
  }

  ID<BlockInfo> MapBlock(Block *block)
  {
    return objs_->MapInst(cs_, &*block->begin());
  }

  ID<BlockInfo> MapInst(Inst *inst)
  {
    return objs_->MapInst(cs_, inst);
  }

  ID<BlockInfo> Exit(Func *func)
  {
    return objs_->Exit(cs_, func);
  }

private:
  TaintedObjects *objs_;
  ID<BlockInfo> id_;
  BlockInfo *&info_;
  const TaintedObjects::CallString &cs_;
};

// -----------------------------------------------------------------------------
TaintedObjects::TaintedObjects(Func &entry)
  : single_(SingleExecution(entry).Solve())
  , entry_(Explore(CallString(&entry), entry).Entry)
{
  do {
    Propagate();
  } while (ExpandIndirect());
}

// -----------------------------------------------------------------------------
TaintedObjects::~TaintedObjects()
{
}

// -----------------------------------------------------------------------------
std::optional<TaintedObjects::Tainted> TaintedObjects::operator[](
    Block &block) const
{
  if (auto it = blockSites_.find(&*block.begin()); it != blockSites_.end()) {
    Tainted tainted;
    for (auto blockID : it->second) {
      tainted.Union(blocks_.Map(blockID)->Taint);
    }
    return tainted;
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
TaintedObjects::FunctionID TaintedObjects::Visit(
    const CallString &cs,
    Func &func)
{
  Key<const Func *> key{ cs, &func };
  if (auto it = funcs_.find(key); it != funcs_.end()) {
    return it->second;
  }

  CallString fcs = Context(cs, &func);
  auto entry = MapInst(fcs, &*func.getEntryBlock().begin());
  auto exit = Exit(fcs, &func);
  FunctionID id{ entry, exit };
  funcs_.emplace(key, id);

  for (auto *block : llvm::ReversePostOrderTraversal<Func *>(&func)) {
    ID<BlockInfo> blockID = MapInst(fcs, &*block->begin());
    BlockInfo *blockInfo = blocks_.Map(blockID);

    for (auto &inst : *block) {
      BlockBuilder(this, blockID, blockInfo, fcs).Dispatch(&inst);
    }
  }

  return id;
}

// -----------------------------------------------------------------------------
TaintedObjects::FunctionID TaintedObjects::Explore(
    const CallString &cs,
    Func &func)
{
  auto id = Visit(cs, func);
  while (!explore_.empty()) {
    auto item = explore_.front();
    explore_.pop();

    auto itemID = Visit(item.CS, *item.F);
    item.Site->Successors.Insert(itemID.Entry);
    auto *exit = blocks_.Map(itemID.Exit);
    for (auto cont : item.Cont) {
      exit->Successors.Insert(cont);
    }
  }
  return id;
}

// -----------------------------------------------------------------------------
ID<BlockInfo> TaintedObjects::Explore(const CallString &cs, Block &block)
{
  Inst *entry = &*block.begin();
  Key<const Inst *> key{ cs, entry };
  if (auto it = instToBlock_.find(key); it != instToBlock_.end()) {
    return it->second;
  }

  ID<BlockInfo> blockID = MapInst(cs, entry);
  BlockInfo *blockInfo = blocks_.Map(blockID);
  for (auto &inst : block) {
    BlockBuilder(this, blockID, blockInfo, cs).Dispatch(&inst);
  }
  return blockID;
}

// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<BlockInfo *> {
  using NodeRef = BlockInfo *;
  using ChildIteratorType = BlockInfo::iterator;

  static NodeRef getEntryNode(BlockInfo* BB) { return BB; }
  static ChildIteratorType child_begin(NodeRef N) { return N->begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->end(); }
};

// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<TaintedObjects *>
    : public llvm::GraphTraits<BlockInfo *>
{
  static NodeRef getEntryNode(TaintedObjects *G) { return G->GetEntryNode(); }
};

// -----------------------------------------------------------------------------
void TaintedObjects::Propagate()
{
  for (auto it = llvm::scc_begin(this); !it.isAtEnd(); ++it) {
    if (it->size() > 1) {
      std::vector<ID<BlockInfo>> blocks;
      for (auto it : *it) {
        blocks.push_back(it->BlockID);
      }

      auto id = blocks[0];
      for (unsigned i = 1, n = blocks.size(); i < n; ++i) {
        id = blocks_.Union(id, blocks[i]);
      }
      queue_.Push(id);
    }
  }

  while (!queue_.Empty()) {
    auto nodeID = queue_.Pop();
    BlockInfo *node = blocks_.Map(nodeID);
    if (node->BlockID != nodeID) {
      continue;
    }

    std::vector<ID<BlockInfo>> fixups;
    for (auto succID : node->Successors) {
      BlockInfo *succ = blocks_.Map(succID);
      if (succ->BlockID != succID) {
        continue;
      }
      if (succ->Taint.Union(node->Taint)) {
        queue_.Push(succID);
      }
    }
  }
}

// -----------------------------------------------------------------------------
bool TaintedObjects::ExpandIndirect()
{
  bool changed = false;

  // Expand indirect jumps.
  {
    std::set<BlockInfo *> expandedJumps;
    auto indirectJumps = indirectJumps_;
    for (auto &jump : indirectJumps) {
      auto *node = blocks_.Map(jump.From);
      if (!expandedJumps.insert(node).second) {
        continue;
      }
      bool expanded = false;
      for (auto blockID : node->Taint.blocks()) {
        auto *block = blockMap_.Map(blockID);
        auto id = Explore(jump.CS.Indirect(), *block);
        if (node->Successors.Insert(id)) {
          expanded = true;
        }
      }
      if (expanded) {
        changed = true;
        queue_.Push(jump.From);
      }
    }
  }

  // Expand indirect calls.
  {
    std::set<BlockInfo *> expandedCalls;
    auto indirectCalls = indirectCalls_;
    for (auto &call : indirectCalls) {
      auto *node = blocks_.Map(call.From);
      if (!expandedCalls.insert(node).second) {
        continue;
      }

      bool expanded = false;
      std::set<BlockInfo *> expandedConts;
      for (auto c : call.Cont) {
        if (!expandedConts.insert(blocks_.Map(c)).second) {
          continue;
        }
        for (auto funcID : node->Taint.funcs()) {
          auto id = Explore(call.CS.Indirect(), *funcMap_.Map(funcID));
          auto *ret = blocks_.Map(id.Exit);
          if (node->Successors.Insert(id.Entry) || ret->Successors.Insert(c)) {
            expanded = true;
          }
        }
      }
      if (expanded) {
        changed = true;
        queue_.Push(call.From);
      }
    }
  }

  return changed;
}

// -----------------------------------------------------------------------------
ID<BlockInfo> TaintedObjects::MapInst(const CallString &cs, Inst *inst)
{
  Key<const Inst *> key{ cs, inst };
  if (auto it = instToBlock_.find(key); it != instToBlock_.end()) {
    return it->second;
  }

  auto id = blocks_.Emplace(this);
  instToBlock_.insert({key, id});
  blockSites_[inst].Insert(id);
  return id;
}

// -----------------------------------------------------------------------------
ID<BlockInfo> TaintedObjects::Exit(const CallString &cs, Func *func)
{
  Key<const Func *> key{ cs, func };
  if (auto it = exitToBlock_.find(key); it != exitToBlock_.end()) {
    return it->second;
  }

  auto id = blocks_.Emplace(this);
  exitToBlock_.insert({key, id});
  return id;
}

// -----------------------------------------------------------------------------
TaintedObjects::CallString TaintedObjects::Context(
    const CallString &cs,
    Func *func)
{
  if (single_.count(&func->getEntryBlock())) {
    return cs.Context(func);
  }
  return cs;
}
