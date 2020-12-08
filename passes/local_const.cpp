// This file if part of the llir-opt project.
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
#include "passes/local_const/context.h"
#include "passes/local_const/builder.h"
#include "passes/local_const/graph.h"
#include "passes/local_const/scc.h"
#include "passes/local_const/store_elimination.h"
#include "passes/local_const/store_propagation.h"


/*
// -----------------------------------------------------------------------------
class LocalConstantPropagation {
public:
  LocalConstantPropagation(Func &func)
    : func_(func)
    , blockOrder_(&func_)
    , context_(func, graph_)
    , scc_(graph_)
  {
  }

  void Run()
  {
    BuildGraph();
    SolveGraph();

    StorePropagation(func_, context_).Propagate();
    StoreElimination(func_, context_).Eliminate();
  }

private:
  /// Traverses the method and builds a constraint graph.
  void BuildGraph();
  /// Propagates values throughout the graph.
  void SolveGraph();

private:
  /// Function under optimisation.
  Func &func_;
  /// Block order computed once.
  llvm::ReversePostOrderTraversal<Func *> blockOrder_;

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
        case Inst::Kind::TAIL_CALL: {
          builder.BuildCall(inst);
          continue;
        }
        // Returns must keep escaping pointers live.
        case Inst::Kind::RETURN: {
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
        case Inst::Kind::X86_XCHG: {
          builder.BuildX86_Xchg(static_cast<X86_XchgInst &>(inst));
          continue;
        }
        // Atomic exchange.
        case Inst::Kind::X86_CMP_XCHG: {
          builder.BuildX86_CmpXchg(static_cast<X86_CmpXchgInst &>(inst));
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
*/
