// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <llvm/Support/raw_ostream.h>

#include "core/inst_visitor.h"
#include "core/target.h"
#include "passes/tags/tagged_type.h"



namespace tags {
class Init;
class Step;

class TypeAnalysis {
public:
  TypeAnalysis(Prog &prog, const Target *target)
    : prog_(prog)
    , target_(target)
  {
  }

  void Solve();

  /// Dump the results of the analysis.
  void dump(llvm::raw_ostream &os = llvm::errs());
  
  /// Find the type assigned to a vreg.
  TaggedType Find(ConstRef<Inst> ref)
  {
    auto it = types_.find(ref);
    return it == types_.end() ? TaggedType::Unknown() : it->second;
  }

private:
  friend class Init;
  friend class Step;

  /// Mark an instruction with a type.
  bool Mark(ConstRef<Inst> inst, const TaggedType &type);
  /// Mark operators with a type.
  bool Mark(Inst &inst, const TaggedType &type);
  /// Queue the users of an instruction to be updated.
  void Enqueue(ConstRef<Inst> inst);

private:
  /// Reference to the underlying program.
  Prog &prog_;
  /// Reference to the target arch.
  const Target *target_;
  /// Queue of instructions to propagate information from.
  std::queue<const Inst *> queue_;
  /// Queue of PHI nodes, evaluated after other instructions.
  std::queue<const PhiInst *> phiQueue_;
  /// Set of instructions in the queue.
  std::unordered_set<const Inst *> inQueue_;
  /// Mapping from instructions to their types.
  std::unordered_map<ConstRef<Inst>, TaggedType> types_;
  /// Mapping from indices to arguments.
  std::unordered_map
    < std::pair<const Func *, unsigned>
    , std::vector<const ArgInst *>
    > args_;
  /// Mapping from functions to their return values.
  std::unordered_map< const Func *, std::vector<TaggedType>> rets_;
};

} // end namespace
