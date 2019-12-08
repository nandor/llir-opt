// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>

#include <llvm/ADT/PostOrderIterator.h>

#include "core/adt/bitset.h"
#include "core/adt/queue.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/local_const.h"
#include "passes/local_const/analysis.h"
#include "passes/local_const/context.h"
#include "passes/local_const/builder.h"
#include "passes/local_const/graph.h"
#include "passes/local_const/scc.h"



// -----------------------------------------------------------------------------
class LocalConstantPropagation {
public:
  LocalConstantPropagation(Func &func)
    : func_(func)
    , blockOrder_(&func_)
    , context_(graph_)
    , scc_(graph_)
    , analysis_(func, context_)
  {
  }

  void Run()
  {
    BuildGraph();
    SolveGraph();
    BuildFlow();
    Propagate();
    RemoveDeadStores();
  }

private:
  /// Traverses the method and builds a constraint graph.
  void BuildGraph();
  /// Propagates values throughout the graph.
  void SolveGraph();
  /// Computes RD and LVA per-block.
  void BuildFlow();
  /// Propagates load using RD.
  void Propagate();
  /// Removes stores based on LVA.
  void RemoveDeadStores();

  // Helper method to check if a call is an allocation.
  bool IsAlloc(const Inst &call);

private:
  /// Function under optimisation.
  Func &func_;
  /// Block order computed once.
  llvm::ReversePostOrderTraversal<Func*> blockOrder_;

  /// Constraint graph.
  LCGraph graph_;
  /// Context for each function.
  LCContext context_;
  /// SCC solver.
  LCSCC scc_;
  /// Mapping from instructions to nodes.
  std::unordered_map<const Inst *, ID<LCSet>> nodes_;
  /// Queue of nodes.
  Queue<LCSet> queue_;

  /// Underlying analysis.
  Analysis analysis_;
};

// -----------------------------------------------------------------------------
void LocalConstantPropagation::BuildGraph() {
  GraphBuilder builder(context_, func_, queue_);
  for (Block *block : blockOrder_) {
    for (Inst &inst : *block) {
      switch (inst.GetKind()) {
        // Ignore other instructions.
        default: {
          continue;
        }
        // Potential allocation sites or value/block producing instructions.
        case Inst::Kind::CALL:
        case Inst::Kind::INVOKE:
        case Inst::Kind::TCALL:
        case Inst::Kind::TINVOKE: {
          builder.BuildCall(inst);
          continue;
        }
        // Returns must keep escaping pointers live.
        case Inst::Kind::RET: {
          builder.BuildReturn(static_cast<ReturnInst &>(inst));
          continue;
        }
        // Static stack allocation site.
        case Inst::Kind::FRAME: {
          builder.BuildFrame(static_cast<FrameInst &>(inst));
          continue;
        }
        // Argument introducing blocks_/values.
        case Inst::Kind::ARG: {
          builder.BuildArg(static_cast<ArgInst &>(inst));
          continue;
        }
        // Memory load.
        case Inst::Kind::LD: {
          builder.BuildLoad(static_cast<LoadInst &>(inst));
          continue;
        }
        // Memory store.
        case Inst::Kind::ST: {
          builder.BuildStore(static_cast<StoreInst &>(inst));
          continue;
        }
        // MOV propagating values.
        case Inst::Kind::MOV: {
          auto *movArg = static_cast<MovInst &>(inst).GetArg();
          switch (movArg->GetKind()) {
            case Value::Kind::INST: {
              builder.BuildMove(inst, static_cast<Inst *>(movArg));
              continue;
            }
            case Value::Kind::GLOBAL: {
              builder.BuildExtern(inst, static_cast<Global *>(movArg));
              continue;
            }
            case Value::Kind::EXPR: {
              switch (static_cast<Expr *>(movArg)->GetKind()) {
                case Expr::Kind::SYMBOL_OFFSET: {
                  auto *e = static_cast<SymbolOffsetExpr *>(movArg);
                  builder.BuildExtern(inst, e->GetSymbol());
                  continue;
                }
              }
              continue;
            }
            case Value::Kind::CONST: {
              continue;
            }
          }
          llvm_unreachable("invalid argument kind");
        }
        // PHI node merging paths.
        case Inst::Kind::PHI: {
          builder.BuildPhi(static_cast<PhiInst &>(inst));
          continue;
        }
        // ADD and SUB offset constants.
        case Inst::Kind::ADD: {
          builder.BuildAdd(static_cast<AddInst &>(inst));
          continue;
        }
        case Inst::Kind::SUB: {
          builder.BuildSub(static_cast<SubInst &>(inst));
          continue;
        }
        // AND and OR propagate all values.
        case Inst::Kind::AND:
        case Inst::Kind::OR: {
          builder.BuildFlow(static_cast<BinaryInst &>(inst));
          continue;
        }
        // Dynamic stack allocation site.
        case Inst::Kind::ALLOCA: {
          builder.BuildAlloca(static_cast<AllocaInst &>(inst));
          continue;
        }
        // Atomic exchange.
        case Inst::Kind::XCHG: {
          builder.BuildXchg(static_cast<ExchangeInst &>(inst));
          continue;
        }
        // Vararg - unify the whole range of the pointer with the extern set.
        case Inst::Kind::VASTART: {
          builder.BuildVAStart(static_cast<VAStartInst &>(inst));
          continue;
        }
        // Ternary instruction producing both pointers and values.
        case Inst::Kind::SELECT: {
          builder.BuildSelect(static_cast<SelectInst &>(inst));
          continue;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void LocalConstantPropagation::SolveGraph()
{
  // Only trigger a single scc search for an edge.
  std::unordered_set<std::pair<ID<LCSet>, ID<LCSet>>> visited;

  while (!queue_.Empty()) {
    if (LCSet *from = graph_.Get(queue_.Pop())) {
      // Look at the dereferenced node's points-to set and add load/store edges.
      if (LCDeref *deref = from->Deref()) {
        from->points_to_set([this, deref](ID<LCSet> inID, ID<LCSet> outID) {
          LCSet *in = graph_.Find(inID);
          deref->set_ins([this, in](LCSet *store) {
            if (store->Edge(in)) {
              queue_.Push(store->GetID());
            }
          });
          LCSet *out = graph_.Find(outID);
          deref->set_outs([this, out](LCSet *load) {
            if (out->Edge(load)) {
              queue_.Push(out->GetID());
            }
          });
        });
      }

      // Propagate full ranges to other nodes.
      from->ranges([this, from](LCSet *rangeTo) {
        bool c = false;
        from->points_to_range([rangeTo, &c](LCAlloc *a) {
          if (rangeTo->AddRange(a)) {
            c = true;
          }
        });
        from->points_to_elem([rangeTo, &c](LCAlloc *a, LCIndex) {
          if (rangeTo->AddRange(a)) {
            c = true;
          }
        });
        if (c) {
          queue_.Push(rangeTo->GetID());
        }
      });

      // Propagate element offsets to other nodes.
      from->offsets([this, from](LCSet *to, int64_t off) {
        bool c = false;
        from->points_to_range([to = to, &c](LCAlloc *a) {
          if (to->AddRange(a)) {
            c = true;
          }
        });
        from->points_to_elem([to=to, off=off, &c](LCAlloc *a, LCIndex idx) {
          if (auto newOffset = a->Offset(idx, off)) {
            if (to->AddElement(a, *newOffset)) {
              c = true;
            }
          } else {
            if (to->AddRange(a)) {
              c = true;
            }
          }
        });
        if (c) {
          queue_.Push(to->GetID());
        }
      });

      // Propagate the points-to set.
      {
        bool collapse = false;

        from->sets([this, from, &visited, &collapse](LCSet *to) {
          if (visited.insert({ from->GetID(), to->GetID() }).second) {
            if (from->Equals(to)) {
              collapse = true;
            }
          }
          if (from->Propagate(to)) {
            queue_.Push(to->GetID());
          }
        });

        if (collapse) {
          scc_.Single(from).Solve([this](auto &sets, auto &derefs) {
            auto it = sets.begin();
            if (it == sets.end()) {
              return;
            }
            ID<LCSet> united = *it;
            while (++it != sets.end()) {
              united = graph_.Union(united, *it);
            }
            queue_.Push(united);
          });
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void LocalConstantPropagation::BuildFlow()
{
  // Build kill/gen for individual blocks.
  for (Block *block : blockOrder_) {
    // Build kill/gen for individual instructions.
    for (Inst &inst : *block) {
      switch (inst.GetKind()) {
        // Reaching defs - everything is clobbered.
        // LVA - everithing is defined.
        case Inst::Kind::CALL:
        case Inst::Kind::TCALL:
        case Inst::Kind::INVOKE:
        case Inst::Kind::TINVOKE: {
          if (IsAlloc(inst)) {
            analysis_.BuildAlloc(&inst);
          } else {
            analysis_.BuildCall(&inst);
          }
          break;
        }
        // Reaching defs - nothing is clobbered.
        // LVA - Result of ret is defined.
        case Inst::Kind::JI:
        case Inst::Kind::RET: {
          if (auto *set = context_.GetNode(&inst)) {
            analysis_.BuildGen(&inst, set);
          }
          analysis_.BuildGen(&inst, context_.Root());
          analysis_.BuildGen(&inst, context_.Extern());
          break;
        }
        // The store instruction either defs or clobbers.
        // Reaching defs - def if store to unique pointer.
        // LVA - kill the set stored to.
        case Inst::Kind::ST: {
          auto &st = static_cast<StoreInst &>(inst);
          auto *addr = context_.GetNode(st.GetAddr());
          assert(addr && "missing pointer for set");
          analysis_.BuildStore(&st, addr);
          break;
        }
        // Reaching defs - always clobber.
        // LVA - def and kill the pointer set.
        case Inst::Kind::XCHG: {
          auto *addr = context_.GetNode(static_cast<ExchangeInst &>(inst).GetAddr());
          assert(addr && "missing set for xchg");
          analysis_.BuildClobber(&inst, addr);
          break;
        }
        // The vastart instruction clobbers.
        case Inst::Kind::VASTART: {
          auto *addr = context_.GetNode(static_cast<VAStartInst &>(inst).GetVAList());
          assert(addr && "missing address for vastart");
          analysis_.BuildClobber(&inst, addr);
          break;
        }
        // Reaching defs - no clobber.
        // LVA - def the pointer set.
        case Inst::Kind::LD: {
          if (auto *addr = context_.GetNode(static_cast<LoadInst &>(inst).GetAddr())) {
            analysis_.BuildGen(&inst, addr);
          }
          break;
        }
        case Inst::Kind::OR:
        case Inst::Kind::AND:
        case Inst::Kind::ADD: {
          auto &binInst = static_cast<BinaryInst &>(inst);
          break;
        }
        case Inst::Kind::PHI: {
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  analysis_.Solve();
}

// -----------------------------------------------------------------------------
void LocalConstantPropagation::Propagate()
{
  analysis_.ReachingDefs([this](Inst * I, const Analysis::ReachSet &defs) {
    if (auto *ld = ::dyn_cast_or_null<LoadInst>(I)) {
      auto *set = context_.GetNode(ld->GetAddr());
      assert(set && "missing pointer set for load");

      // See if the load is from a unique address.
      std::optional<Element> elem;
      set->points_to_elem([&elem](LCAlloc *alloc, uint64_t idx) {
        if (elem) {
          elem = std::nullopt;
        } else {
          elem = { alloc->GetID(), idx };
        }
      });
      set->points_to_range([&elem](LCAlloc *alloc) {
        elem = std::nullopt;
      });
      if (!elem) {
        return;
      }

      // Find a store which can be propagated.
      if (auto st = defs.Find(*elem)) {
        if (st->GetStoreSize() != ld->GetLoadSize()) {
          return;
        }

        // Check if the argument can be propagated.
        auto val = st->GetVal();
        if (val->GetType(0) != ld->GetType()) {
          return;
        }

        ld->replaceAllUsesWith(val);
        ld->eraseFromParent();
      }
    }
  });
}

// -----------------------------------------------------------------------------
void LocalConstantPropagation::RemoveDeadStores()
{
  analysis_.LiveStores([this](Inst *I, const Analysis::LiveSet &live) {
    if (auto *store = ::dyn_cast_or_null<StoreInst>(I)) {
      auto *set = context_.GetNode(store->GetAddr());
      assert(set && "missing set for store");

      // Check if the store writes to a live location.
      bool isLive = false;
      set->points_to_elem([&](LCAlloc *alloc, uint64_t index) {
        auto allocID = alloc->GetID();
        isLive |= live.Contains(allocID, index);
        isLive |= live.Contains(allocID);
      });
      set->points_to_range([&](LCAlloc *alloc) {
        isLive |= live.Contains(alloc->GetID());
      });

      // If not, erase it.
      if (!isLive) {
        store->eraseFromParent();
      }
    }
  });
}

// -----------------------------------------------------------------------------
bool LocalConstantPropagation::IsAlloc(const Inst &call)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(call.Op<0>())) {
    if (auto *callee = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
      const auto &name = callee->getName();
      if (name.substr(0, 10) == "caml_alloc") {
        return true;
      }
      if (name == "malloc") {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
const char *LocalConstPass::kPassID = "local-const";

// -----------------------------------------------------------------------------
void LocalConstPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    LocalConstantPropagation(func).Run();
  }
}


// -----------------------------------------------------------------------------
const char *LocalConstPass::GetPassName() const
{
  return "Local Constant Propagation";
}
