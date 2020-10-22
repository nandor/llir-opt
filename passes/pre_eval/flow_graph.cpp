// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <unordered_set>

#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/call_graph.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/object_graph.h"
#include "passes/pre_eval/flow_graph.h"


/*
// -----------------------------------------------------------------------------
static bool AlwaysCalled(const Inst *inst)
{
  for (const User *user : inst->users()) {
    auto *userValue = static_cast<const Value *>(user);
    if (auto *userInst = ::cast_or_null<const Inst>(userValue)) {
      switch (userInst->GetKind()) {
        case Inst::Kind::CALL:
        case Inst::Kind::TCALL:
        case Inst::Kind::INVOKE: {
          auto &site = static_cast<const CallSite &>(*userInst);
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
FlowGraph::FlowGraph(Prog &prog)
{
  // Build the object graph and traverse it in topological order.
  // For each SCC, collect all symbols referenced in it, along with
  // symbols from other transitively connected objects.
  ObjectGraph og(prog);
  for (auto it = llvm::scc_begin(&og); !it.isAtEnd(); ++it) {
    // Create an object to track references and attach it to the SCC.
    auto refs = std::make_shared<ObjectRefs>();
    for (auto *node : *it) {
      if (auto *o = node->GetObject()) {
        objRefs_.emplace(o, refs);
      }
    }

    // Populate the object with symbols referenced in the SCC.
    for (auto *node : *it) {
      if (auto *o = node->GetObject()) {
        refs->Objects.Insert(objectMap_[o]);
        for (const Atom &atom : *o) {
          for (const Item &item : atom) {
            if (auto *expr = item.AsExpr()) {
              switch (expr->GetKind()) {
                case Expr::Kind::SYMBOL_OFFSET: {
                  auto *sym = static_cast<SymbolOffsetExpr *>(expr)->GetSymbol();
                  switch (sym->GetKind()) {
                    case Global::Kind::EXTERN: {
                      // TODO: handle externs which capture pointers.
                      continue;
                    }
                    case Global::Kind::FUNC: {
                      refs->Funcs.Insert(funcMap_[static_cast<Func *>(sym)]);
                      continue;
                    }
                    case Global::Kind::BLOCK: {
                      refs->Blocks.Insert(blockMap_[static_cast<Block *>(sym)]);
                      continue;
                    }
                    case Global::Kind::ATOM: {
                      auto object = static_cast<Atom *>(sym)->getParent();
                      auto objRefs = objRefs_.find(object);
                      assert(objRefs != objRefs_.end() && "invalid reference");
                      if (refs != objRefs->second) {
                        refs->Funcs.Union(objRefs->second->Funcs);
                        refs->Blocks.Union(objRefs->second->Blocks);
                        refs->Objects.Union(objRefs->second->Objects);
                      }
                      continue;
                    }
                  }
                }
              }
              llvm_unreachable("invalid expression kind");
            }
          }
        }
      }
    }
  }

  // Build the shadow control flow graph.
  CallGraph cg(prog);
  for (auto it = llvm::scc_begin(&cg); !it.isAtEnd(); ++it) {
    // Create an object to track references in the loop.
    auto refs = std::make_shared<FunctionRefs>();
    for (auto *node : *it) {
      if (auto *f = node->GetCaller()) {
        funcRefs_.emplace(f, refs);
      }
    }

    // Populate the object with all referenced symbols.
    for (auto *node : *it) {
      if (auto *f = node->GetCaller()) {
        for (const Block &block : *f) {
          for (const Inst &inst : block) {
            ExtractRefs(inst, *refs);
          }
        }
      }
    }

    // Build nodes from the blocks of the functions.
    if (it->size() > 1 || (*it)[0]->IsRecursive()) {
      std::set<const Func *> funcs;
      for (const auto *node : *it) {
        if (auto *f = node->GetCaller()) {
          funcs.insert(f);
        }
      }
      BuildLoop(funcs);
    } else {
      if (auto *f = (*it)[0]->GetCaller()) {
        BuildNode(*f);
      }
    }
  }
}

// -----------------------------------------------------------------------------
ID<FlowGraph::Node> FlowGraph::operator[](const Func *func)
{
  auto it = funcs_.find(func);
  assert(it != funcs_.end() && "missing function");
  return it->second;
}

// -----------------------------------------------------------------------------
const std::unordered_map<std::string, bool> kCallbacks =
{
  #define SYSCALL(name, callback) { #name, callback },
  #include "core/syscalls.h"
  #undef SYSCALL
};

// -----------------------------------------------------------------------------
const std::unordered_set<std::string> kOCamlAlloc =
{
  "caml_alloc1",
  "caml_alloc2",
  "caml_alloc3",
  "caml_allocN",
  "caml_alloc",
  "caml_alloc_custom",
  "caml_stat_alloc",
  "caml_stat_resize",
  "caml_stat_free",
  "caml_stat_strconcat",
  "caml_create_bytes",
  "caml_alloc_young",
  "caml_alloc_shr",
  "caml_ml_open_descriptor_in",
  "caml_ml_open_descriptor_out",
  "caml_copy_string",
  "caml_alloc_array",
};

// -----------------------------------------------------------------------------
void FlowGraph::ExtractRefs(const Inst &inst, FunctionRefs &refs)
{
  switch (inst.GetKind()) {
    case Inst::Kind::RAISE: {
      refs.HasIndirectJumps = true;
      return;
    }
    case Inst::Kind::MOV: {
      ExtractRefsMove(static_cast<const MovInst &>(inst), refs);
      return;
    }
    case Inst::Kind::CALL:
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE: {
      auto &call = static_cast<const CallSite &>(inst);
      ExtractRefsCallee(call.GetCallee(), refs);
      return;
    }
    default: {
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void FlowGraph::ExtractRefsMove(const MovInst &inst, FunctionRefs &refs)
{
  auto *arg = inst.GetArg();
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
          refs.Blocks.Insert(blockMap_[static_cast<Block *>(arg)]);
          return;
        }
        case Global::Kind::FUNC: {
          if (!AlwaysCalled(&inst)) {
            refs.Funcs.Insert(funcMap_[static_cast<Func *>(arg)]);
          }
          return;
        }
        case Global::Kind::ATOM: {
          ExtractRefsAtom(static_cast<const Atom *>(arg), refs);
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
            case Global::Kind::EXTERN: {
              // Externs do not carry additional information.
              return;
            }
            case Global::Kind::BLOCK:
            case Global::Kind::FUNC: {
              // Pointers into functions/blocks are UB.
              return;
            }
            case Global::Kind::ATOM: {
              ExtractRefsAtom(static_cast<const Atom *>(sym), refs);
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

// -----------------------------------------------------------------------------
void FlowGraph::ExtractRefsAtom(const Atom *atom, FunctionRefs &refs)
{
  auto objRefsIt = objRefs_.find(atom->getParent());
  assert(objRefsIt != objRefs_.end() && "invalid reference");
  auto &objRefs = objRefsIt->second;
  refs.Funcs.Union(objRefs->Funcs);
  refs.Blocks.Union(objRefs->Blocks);
  refs.Objects.Union(objRefs->Objects);
}

// -----------------------------------------------------------------------------
void FlowGraph::ExtractRefsCallee(const Inst *callee, FunctionRefs &refs)
{
  if (auto *mov = ::cast_or_null<const MovInst>(callee)) {
    auto *callee = mov->GetArg();
    switch (callee->GetKind()) {
      case Value::Kind::INST: {
        refs.HasIndirectCalls = true;
        return;
      }
      case Value::Kind::GLOBAL: {
        switch (static_cast<Global *>(callee)->GetKind()) {
          case Global::Kind::EXTERN: {
            auto *ext = static_cast<Extern *>(callee);
            std::string name(ext->GetName());
            auto it = kCallbacks.find(name);
            if (it == kCallbacks.end() || it->second) {
              std::ostringstream os;
              os << "not implemented: " << name;
              llvm_unreachable(os.str().c_str());
            } else {
              return;
            }
          }
          case Global::Kind::FUNC: {
            auto *func = static_cast<const Func *>(callee);
            if (kOCamlAlloc.count(std::string(func->getName()))) {
              return;
            }
            auto funcRefsIt = funcRefs_.find(func);
            assert(funcRefsIt != funcRefs_.end() && "invalid reference");
            auto &funcRefs = funcRefsIt->second;
            refs.Funcs.Union(funcRefs->Funcs);
            refs.Blocks.Union(funcRefs->Blocks);
            refs.Objects.Union(funcRefs->Objects);
            refs.HasIndirectJumps |= funcRefs->HasIndirectJumps;
            refs.HasIndirectCalls |= funcRefs->HasIndirectCalls;
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
    refs.HasIndirectCalls = true;
    return;
  }
}

// -----------------------------------------------------------------------------
const Func *FlowGraph::BuildCallRefs(const Inst *callee, FunctionRefs &refs)
{
  if (auto *mov = ::cast_or_null<const MovInst>(callee)) {
    auto *callee = mov->GetArg();
    switch (callee->GetKind()) {
      case Value::Kind::INST: {
        refs.HasIndirectCalls = true;
        return nullptr;
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
              return nullptr;
            }
          }
          case Global::Kind::FUNC: {
            auto *func = static_cast<const Func *>(callee);
            if (kOCamlAlloc.count(std::string(func->getName()))) {
              return nullptr;
            }
            return func;
          }
          case Global::Kind::BLOCK:
          case Global::Kind::ATOM: {
            // Undefined behaviour - no flow.
            return nullptr;
          }
        }
        llvm_unreachable("invalid global kind");
      }
      case Value::Kind::CONST:
      case Value::Kind::EXPR: {
        // Undefined behaviour - no flow.
        return nullptr;
      }
    }
    llvm_unreachable("invalid value kind");
  } else {
    refs.HasIndirectCalls = true;
    return nullptr;
  }
}

// -----------------------------------------------------------------------------
static bool SelfLoop(const Block *block)
{
  for (const Block *succ : block->successors()) {
    if (succ == block) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
ID<FlowGraph::Node> FlowGraph::CreateNode(
    FunctionRefs &&refs,
    BitSet<Inst> origins,
    const Func *callee,
    bool IsLoop,
    bool IsExit)
{
  ID<Node> nodeID = nodes_.size();

  Node node;
  node.Callee = callee;
  node.Funcs = std::move(refs.Funcs);
  node.Blocks = std::move(refs.Blocks);
  node.Objects = std::move(refs.Objects);
  node.Origins = std::move(origins);
  node.HasIndirectJumps = refs.HasIndirectJumps;
  node.HasIndirectCalls = refs.HasIndirectCalls;
  node.IsLoop = IsLoop;
  node.IsExit = IsExit;
  nodes_.emplace_back(std::move(node));

  return nodeID;
}

// -----------------------------------------------------------------------------
void FlowGraph::BuildNode(const Func &func)
{
  struct FlowBlock {
    /// Items referenced in the block.
    FunctionRefs Refs;
    /// Flag indicating if this is a self loop.
    bool IsLoop;
    /// Function called, if there is one.
    const Func *Callee;
    /// Originating instructions.
    BitSet<Inst> Origins;
    /// Successor nodes.
    BitSet<FlowBlock> Successors;

    /// Creates the exit node.
    FlowBlock() : IsLoop(false), Callee(nullptr) { }

    /// Creates a new loop node.
    FlowBlock(FunctionRefs &&refs, BitSet<Inst> &&origins)
      : Refs(std::move(refs))
      , IsLoop(true)
      , Callee(nullptr)
      , Origins(std::move(origins))
    {
    }

    /// Creates a new node with no successors.
    FlowBlock(
        FunctionRefs &&refs,
        ID<Inst> origin,
        const Func *callee)
      : Refs(std::move(refs))
      , IsLoop(false)
      , Callee(callee)
      , Origins({ origin })
    {
    }

    /// Node cannot be eliminated.
    bool Anchored() const {
      return IsLoop || Callee || Refs.HasIndirectJumps || Refs.HasIndirectCalls;
    }
  };

  // Flow graph from basic blocks.
  std::vector<FlowBlock> blocks;
  std::unordered_map<const Block *, ID<FlowBlock>> blockIDs;

  // ID of the exit node.
  auto exitID = blocks.size();
  blocks.emplace_back();

  // Convert the CFG to the simplified flow graph.
  for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
    if (it->size() > 1 || SelfLoop((*it)[0])) {
      // A loop inside a function: extract all references from the nodes and
      // any functions invoked from any of the instructions in the loop.
      FunctionRefs refs;
      BitSet<Inst> origins;
      bool isExit = false;
      for (const Block *block : *it) {
        origins.Insert(instMap_[&*block->begin()]);
        for (const Inst &inst : *block) {
          ExtractRefs(inst, refs);
        }
        isExit |= block->GetTerminator()->IsReturn();
      }

      // Create the loop node and attach it to the blocks.
      ID<FlowBlock> loopID = blocks.size();
      blocks.emplace_back(std::move(refs), std::move(origins));
      for (const Block *block : *it) {
        blockIDs.emplace(block, loopID);
      }
      if (isExit) {
        blocks.rbegin()->Successors.Insert(exitID);
      }

      // Connect to successors.
      FlowBlock &flowBlock = blocks[loopID];
      for (const auto *block : *it) {
        for (const auto *succ : block->successors()) {
          auto it = blockIDs.find(succ);
          assert(it != blockIDs.end() && "missing block");
          flowBlock.Successors.Insert(it->second);
        }
      }
    } else {
      const Block *block = (*it)[0];

      // Split the block at call sites.
      FunctionRefs refs;
      bool isExit = false;
      const Func *callee = nullptr;
      for (const Inst &inst : *block) {
        switch (inst.GetKind()) {
          case Inst::Kind::MOV: {
            ExtractRefsMove(static_cast<const MovInst &>(inst), refs);
            continue;
          }
          case Inst::Kind::CALL: {
            auto &call = static_cast<const CallSite &>(inst);
            callee = BuildCallRefs(call.GetCallee(), refs);
            continue;
          }
          case Inst::Kind::TCALL:
          case Inst::Kind::INVOKE: {
            auto &call = static_cast<const CallSite &>(inst);
            callee = BuildCallRefs(call.GetCallee(), refs);
            isExit = true;
            continue;
          }
          case Inst::Kind::RAISE: {
            refs.HasIndirectJumps = true;
            break;
          }
          case Inst::Kind::JCC:
          case Inst::Kind::JMP:
          case Inst::Kind::SWITCH:
          case Inst::Kind::TRAP: {
            break;
          }
          case Inst::Kind::RET: {
            isExit = true;
            break;
          }
          default: {
            continue;
          }
        }
      }

      // Add the splits in reverse order.
      auto blockID = blocks.size();
      blocks.emplace_back(
        std::move(refs),
        instMap_[&*block->begin()],
        callee
      );
      FlowBlock &flowBlock = *blocks.rbegin();
      if (isExit) {
        flowBlock.Successors.Insert(exitID);
      }

      // Add successors, connecting the last split to the block successors.
      for (const Block *succ : block->successors()) {
        auto it = blockIDs.find(succ);
        assert(it != blockIDs.end() && "missing block");
        flowBlock.Successors.Insert(it->second);
      }

      // Save the block ID.
      blockIDs.emplace(block, blockID);
    }
  }
  // Find the entry node.
  auto it = blockIDs.find(&func.getEntryBlock());
  assert(it != blockIDs.end() && "missing entry block");
  ID<FlowBlock> entryID = it->second;

  // Convert the graph to nodes.
  std::optional<ID<Node>> entryNode;
  std::unordered_map<ID<FlowBlock>, ID<Node>> flowBlockToNodeID;
  for (size_t i = 0; i < blocks.size(); ++i) {
    // Allocate a node.
    auto &block = blocks[i];
    auto nodeID = CreateNode(
        std::move(block.Refs),
        std::move(block.Origins),
        block.Callee,
        block.IsLoop,
        i == exitID
    );
    flowBlockToNodeID.emplace(i, nodeID);

    // Identify entry & exit nodes.
    if (i == entryID) {
      entryNode = nodeID;
    }

    // Record the successors.
    auto &node = nodes_[nodeID];
    for (const auto succID : block.Successors) {
      auto it = flowBlockToNodeID.find(succID);
      assert(it != flowBlockToNodeID.end() && "missing flow block");
      node.Successors.Insert(it->second);
    }

    // Register the blocks.
    for (const auto origin : block.Origins) {
      blocks_.emplace(instMap_[origin], i);
    }
  }

  // Record information about the function.
  assert(entryNode && "missing entry node");
  funcs_.emplace(&func, *entryNode);
}

// -----------------------------------------------------------------------------
void FlowGraph::BuildLoop(const std::set<const Func *> &funcs)
{
  // Combine all references in the loop an any subtree.
  FunctionRefs refs;
  BitSet<Inst> origins;
  for (const Func *f : funcs) {
    for (const Block &block : *f) {
      origins.Insert(instMap_[&*block.begin()]);
      for (const Inst &inst : block) {
        ExtractRefs(inst, refs);
      }
    }
  }
  // Create the node and map the source blocks.
  ID<Node> nodeID = CreateNode(std::move(refs), origins, nullptr, true, true);
  for (const auto origin : origins) {
    blocks_.emplace(instMap_[origin], nodeID);
  }

  // Build a node and map it to the functions.
  for (const Func *func : funcs) {
    funcs_.emplace(func, nodeID);
  }
}
*/
