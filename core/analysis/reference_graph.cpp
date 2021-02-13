// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/ADT/SCCIterator.h>

#include "core/insts.h"
#include "core/block.h"
#include "core/func.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/reference_graph.h"



// -----------------------------------------------------------------------------
ReferenceGraph::ReferenceGraph(Prog &prog, CallGraph &graph)
  : graph_(graph)
  , built_(false)
{
}

// -----------------------------------------------------------------------------
void ReferenceGraph::Build()
{
  for (auto it = llvm::scc_begin(&graph_); !it.isAtEnd(); ++it) {
    auto &node = *nodes_.emplace_back(std::make_unique<Node>());
    for (auto *sccNode : *it) {
      if (auto *func = sccNode->GetCaller()) {
        ExtractReferences(*func, node);
      }
    }
    for (auto *sccNode : *it) {
      if (auto *func = sccNode->GetCaller()) {
        funcToNode_.emplace(func, &node);
      }
    }
  }
}

// -----------------------------------------------------------------------------
static bool HasIndirectUses(MovInst *inst)
{
  std::queue<MovInst *> q;
  q.push(inst);
  while (!q.empty()) {
    MovInst *i = q.front();
    q.pop();
    for (User *user : i->users()) {
      if (auto *mov = ::cast_or_null<MovInst>(user)) {
        q.push(mov);
      } else if (auto *call = ::cast_or_null<CallSite>(user)) {
        if (call->GetCallee().Get() != i) {
          return true;
        }
      } else {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
const ReferenceGraph::Node &ReferenceGraph::operator[](Func &func)
{
  if (!built_) {
    Build();
    built_ = true;
  }
  return *funcToNode_[&func];
}

// -----------------------------------------------------------------------------
void ReferenceGraph::Node::Merge(const Node &that)
{
  HasIndirectCalls |= that.HasIndirectCalls;
  HasRaise |= that.HasRaise;
  HasBarrier |= that.HasBarrier;

  // Merge escapes.
  for (auto *g : that.Escapes) {
    Escapes.insert(g);
  }
  // Merge reads.
  for (auto *g : that.ReadRanges) {
    ReadRanges.insert(g);
  }
  for (auto it = ReadOffsets.begin(); it != ReadOffsets.end(); ) {
    if (ReadRanges.count(it->first)) {
      ReadOffsets.erase(it++);
    } else {
      ++it;
    }
  }
  for (auto &[o, offsets] : that.ReadOffsets) {
    if (ReadRanges.count(o)) {
      continue;
    }
    for (auto off : offsets) {
      ReadOffsets[o].insert(off);
    }
  }

  // Merge writes.
  for (auto *g : that.Written) {
    Written.insert(g);
  }
  for (auto *g : that.Called) {
    Called.insert(g);
  }
  for (auto *b : that.Blocks) {
    Blocks.insert(b);
  }
}

// -----------------------------------------------------------------------------
void ReferenceGraph::ExtractReferences(Func &func, Node &node)
{
  for (Block &block : func) {
    for (Inst &inst : block) {
      if (auto *call = ::cast_or_null<CallSite>(&inst)) {
        if (auto *func = call->GetDirectCallee()) {
          if (!Skip(*func)) {
            if (auto it = funcToNode_.find(func); it != funcToNode_.end()) {
              node.Merge(*it->second);
            }
          }
        } else {
          node.HasIndirectCalls = true;
        }
        continue;
      }
      if (auto *movInst = ::cast_or_null<MovInst>(&inst)) {
        auto extract = [&](Global &g, int64_t offset)
        {
          switch (g.GetKind()) {
            case Global::Kind::FUNC: {
              if (HasIndirectUses(movInst)) {
                node.Escapes.insert(&g);
              } else {
                node.Called.insert(&static_cast<Func &>(g));
              }
              return;
            }
            case Global::Kind::BLOCK: {
              node.Blocks.insert(&static_cast<Block &>(g));
              return;
            }
            case Global::Kind::EXTERN: {
              node.Escapes.insert(&g);
              return;
            }
            case Global::Kind::ATOM: {
              auto *object = static_cast<Atom &>(g).getParent();
              if (g.getName() == "caml_globals") {
                // Not followed here.
              } else {
                if (object->size() == 1) {
                  Classify(object, *movInst, node, offset);
                } else {
                  Classify(object, *movInst, node);
                }
              }
              return;
            }
          }
          llvm_unreachable("invalid global kind");
        };

        auto movArg = movInst->GetArg();
        switch (movArg->GetKind()) {
          case Value::Kind::GLOBAL: {
            extract(*::cast<Global>(movArg), 0);
            continue;
          }
          case Value::Kind::EXPR: {
            switch (::cast<Expr>(movArg)->GetKind()) {
              case Expr::Kind::SYMBOL_OFFSET: {
                auto symExpr = ::cast<SymbolOffsetExpr>(movArg);
                extract(*symExpr->GetSymbol(), symExpr->GetOffset());
                continue;
              }
            }
            llvm_unreachable("invalid expression kind");
          }
          case Value::Kind::INST:
          case Value::Kind::CONST: {
            continue;
          }
        }
        llvm_unreachable("invalid value kind");
      }
      if (auto *raise = ::cast_or_null<RaiseInst>(&inst)) {
        node.HasRaise = true;
        continue;
      }
      if (auto *barrier = ::cast_or_null<BarrierInst>(&inst)) {
        node.HasBarrier = true;
        continue;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ReferenceGraph::Classify(Object *o, const MovInst &inst, Node &node)
{
  std::queue<std::pair<const Inst *, ConstRef<Inst>>> q;
  q.emplace(&inst, nullptr);

  unsigned loadCount = 0;
  unsigned storeCount = 0;
  bool escapes = false;
  std::set<const Inst *> vi;
  while (!q.empty() && !escapes) {
    auto [i, ref] = q.front();
    q.pop();
    if (!vi.insert(i).second) {
      continue;
    }
    switch (i->GetKind()) {
      default: {
        escapes = true;
        continue;
      }
      case Inst::Kind::LOAD: {
        loadCount++;
        continue;
      }
      case Inst::Kind::STORE: {
        auto *store = static_cast<const StoreInst *>(i);
        if (store->GetValue() == ref) {
          escapes = true;
        } else {
          storeCount++;
        }
        continue;
      }
      case Inst::Kind::MOV:
      case Inst::Kind::ADD:
      case Inst::Kind::SUB:
      case Inst::Kind::PHI: {
        for (const User *user : i->users()) {
          if (auto *inst = ::cast_or_null<const Inst>(user)) {
            q.emplace(inst, i);
          }
        }
        continue;
      }
    }
  }

  if (escapes) {
    for (Atom &atom : *o) {
      node.Escapes.insert(&atom);
    }
  } else {
    if (loadCount) {
      node.ReadRanges.insert(o);
    }
    if (storeCount) {
      node.Written.insert(o);
    }
  }
}

// -----------------------------------------------------------------------------
static std::optional<int64_t> GetConstant(ConstRef<Inst> inst)
{
  if (auto movInst = ::cast_or_null<MovInst>(inst)) {
    if (auto movValue = ::cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (movValue->GetValue().getMinSignedBits() >= 64) {
        return movValue->GetInt();
      }
    }
  }
  return {};
}

// -----------------------------------------------------------------------------
void ReferenceGraph::Classify(
    Object *o,
    const MovInst &inst,
    Node &node,
    int64_t offset)
{
  std::queue<std::pair<const Inst *, std::pair<ConstRef<Inst>, std::optional<int64_t>>>> q;
  q.emplace(&inst, std::make_pair(nullptr, std::optional(offset)));

  auto inaccurate = [&] (const Inst *i)
  {
    for (const User *user : i->users()) {
      if (auto *inst = ::cast_or_null<const Inst>(user)) {
        q.emplace(inst, std::make_pair(i, std::nullopt));
      }
    }
  };

  auto accurate = [&] (const Inst *i, int64_t offset)
  {
    for (const User *user : i->users()) {
      if (auto *inst = ::cast_or_null<const Inst>(user)) {
        q.emplace(inst, std::make_pair(i, offset));
      }
    }
  };

  std::set<std::pair<int64_t, int64_t>> loadedOffsets;
  std::set<std::pair<int64_t, int64_t>> storedOffsets;
  bool loadInaccurate = false;
  bool storeInaccurate = false;
  bool escapes = false;

  std::set<const Inst *> vi;
  while (!q.empty() && !escapes) {
    auto [i, refAndStart] = q.front();
    auto [ref, start] = refAndStart;
    q.pop();

    if (!vi.insert(i).second) {
      continue;
    }
    switch (i->GetKind()) {
      default: {
        escapes = true;
        continue;
      }
      case Inst::Kind::LOAD: {
        auto &load = static_cast<const LoadInst &>(*i);
        if (start) {
          loadedOffsets.emplace(*start, *start + GetSize(load.GetType()));
        } else {
          loadedOffsets.clear();
          loadInaccurate = true;
        }
        continue;
      }
      case Inst::Kind::STORE: {
        auto &store = static_cast<const StoreInst &>(*i);
        auto value = store.GetValue();
        if (value == ref) {
          escapes = true;
        } else {
          if (start) {
            storedOffsets.emplace(*start, *start + GetSize(value.GetType()));
          } else {
            storedOffsets.clear();
            storeInaccurate = true;
          }
        }
        continue;
      }
      case Inst::Kind::ADD: {
        auto &add = static_cast<const AddInst &>(*i);
        if (start) {
          if (ref == add.GetLHS()) {
            if (auto off = GetConstant(add.GetRHS())) {
              accurate(i, *start + *off);
            } else {
              inaccurate(i);
            }
          } else if (ref == add.GetRHS()) {
            if (auto off = GetConstant(add.GetLHS())) {
              accurate(i, *start + *off);
            } else {
              inaccurate(i);
            }
          } else {
            llvm_unreachable("invalid add");
          }
        } else {
          inaccurate(i);
        }
        continue;
      }
      case Inst::Kind::SUB: {
        auto &sub = static_cast<const SubInst &>(*i);
        if (start) {
          if (ref == sub.GetLHS()) {
            if (auto off = GetConstant(sub.GetRHS())) {
              accurate(i, *start - *off);
            } else {
              inaccurate(i);
            }
          } else if (ref == sub.GetRHS()) {
            inaccurate(i);
          } else {
            llvm_unreachable("invalid sub");
          }
        } else {
          inaccurate(i);
        }
        continue;
      }
      case Inst::Kind::MOV: {
        if (*start) {
          accurate(i, *start);
        } else {
          inaccurate(i);
        }
        continue;
      }
      case Inst::Kind::PHI: {
        inaccurate(i);
        continue;
      }
    }
  }

  if (escapes) {
    for (Atom &atom : *o) {
      node.Escapes.insert(&atom);
    }
  } else {
    if (loadInaccurate || !loadedOffsets.empty()) {
      if (loadInaccurate) {
        node.ReadRanges.emplace(o);
      } else {
        if (!node.ReadRanges.count(o)) {
          node.ReadOffsets.emplace(o, loadedOffsets);
        }
      }
    }
    if (storeInaccurate || !storedOffsets.empty()) {
      node.Written.emplace(o);
    }
  }
}
