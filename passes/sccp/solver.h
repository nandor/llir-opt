// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <queue>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/func.h"
#include "core/inst_visitor.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/sccp/lattice.h"

class Target;



/**
 * Helper class to compute value for all instructions in the program.
 */
class SCCPSolver : InstVisitor<void> {
public:
  /// Solves constraints for the whole program.
  SCCPSolver(Prog &prog, const Target &target);

  /// Returns a lattice value.
  Lattice &GetValue(Ref<Inst> inst);
  /// Checks if a block is executable.
  bool IsExecutable(const Block &block) { return executable_.count(&block); }

private:
  /// Visits a block.
  void Visit(Block *block)
  {
    for (auto &inst : *block) {
      Visit(inst);
    }
  }

  /// Visits an instruction.
  void Visit(Inst &inst)
  {
    assert(executable_.count(inst.getParent()) && "bb not yet visited");
    Dispatch(inst);
  }

  /// Checks if a call can be evaluated.
  bool CanEvaluate(CallSite &site);
  /// Marks a call return as overdefined.
  void MarkOverdefinedCall(TailCallInst &site);
  /// Calls a function with a given set of arguments.
  void MarkCall(CallSite &site, Func &callee, Block *cont);

  /// Marks a block as executable.
  bool MarkBlock(Block *block);
  /// Marks an edge as executable.
  bool MarkEdge(Inst &inst, Block *to);
  /// Marks an instruction as overdefined.
  bool MarkOverdefined(Inst &inst)
  {
    bool changed = false;
    for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
      changed |= Mark(inst.GetSubValue(i), Lattice::Overdefined());
    }
    return changed;
  }
  /// Marks an instruction as a constant integer.
  bool Mark(Ref<Inst> inst, const Lattice &value);
  /// Marks an instruction as a boolean.
  bool Mark(Ref<Inst> inst, bool flag);

private:
  void VisitArgInst(ArgInst &inst) override;
  void VisitCallInst(CallInst &inst) override;
  void VisitTailCallInst(TailCallInst &inst) override;
  void VisitInvokeInst(InvokeInst &inst) override;
  void VisitReturnInst(ReturnInst &inst) override;
  void VisitLoadInst(LoadInst &inst) override;
  void VisitBinaryInst(BinaryInst &inst) override;
  void VisitCmpInst(CmpInst &inst) override;
  void VisitUnaryInst(UnaryInst &inst) override;
  void VisitJumpInst(JumpInst &inst) override;
  void VisitJumpCondInst(JumpCondInst &inst) override;
  void VisitSwitchInst(SwitchInst &inst) override;
  void VisitSelectInst(SelectInst &inst) override;
  void VisitFrameInst(FrameInst &inst) override;
  void VisitMovInst(MovInst &inst) override;
  void VisitUndefInst(UndefInst &inst) override;
  void VisitPhiInst(PhiInst &inst) override;
  void VisitInst(Inst &inst) override;

  void VisitX86_CpuIdInst(X86_CpuIdInst &inst) override;

private:
  /// Reference to the target.
  const Target &target_;

  /// Worklist for overdefined values.
  std::queue<Inst *> bottomList_;
  /// Worklist for blocks.
  std::queue<Block *> blockList_;
  /// Worklist for instructions.
  std::queue<Inst *> instList_;

  /// Mapping from instructions to values.
  std::unordered_map<Ref<Inst>, Lattice> values_;
  /// Set of known edges.
  std::set<std::pair<Block *, Block *>> edges_;
  /// Set of executable blocks.
  std::set<const Block *> executable_;
  /// Collection of all arguments used by any function.
  std::map<const Func *, std::map<unsigned, std::set<ArgInst *>>> args_;
  /// Call sites which reach a particular function.
  std::map<const Func *, std::set<std::pair<CallSite *, Block *>>> calls_;
  /// Information about results.
  using ResultMap = std::map<unsigned, std::pair<Type, Lattice>>;
  /// Mapping to the return values of a function.
  std::map<const Func *, ResultMap> returns_;
};
