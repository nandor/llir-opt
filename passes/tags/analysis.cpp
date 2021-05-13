// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Format.h>

#include "core/prog.h"
#include "core/printer.h"
#include "passes/tags/analysis.h"
#include "passes/tags/init.h"
#include "passes/tags/step.h"

using namespace tags;



// -----------------------------------------------------------------------------
bool TypeAnalysis::Mark(ConstRef<Inst> inst, const TaggedType &type)
{
  auto it = types_.emplace(inst, type);
  if (it.second) {
    Enqueue(inst);
    return true;
  } else {
    if (it.first->second == type) {
      return false;
    } else {
      if (!(it.first->second < type)) {
        llvm::errs() << it.first->second << " " << type << "\n";
        llvm::errs() << inst->getParent()->getName() << "\n";
      }
      if (!(inst.GetType() != Type::V64 || type <= TaggedType::Val())) {
        llvm::errs() << type << "\n";
      }
      #ifndef NDEBUG
      assert(it.first->second < type && "no convergence");
      assert(inst.GetType() != Type::V64 || type <= TaggedType::Val());
      #endif
      it.first->second = type;
      Enqueue(inst);
      return true;
    }
  }
}

// -----------------------------------------------------------------------------
bool TypeAnalysis::Mark(Inst &inst, const TaggedType &type)
{
  return Mark(inst.GetSubValue(0), type);
}

// -----------------------------------------------------------------------------
void TypeAnalysis::Enqueue(ConstRef<Inst> inst)
{
  for (const Use &use : inst->uses()) {
    if (use.get() == inst) {
      auto *userInst = ::cast<const Inst>(use.getUser());
      if (inQueue_.insert(userInst).second) {
        if (auto *phi = ::cast_or_null<const PhiInst>(userInst)) {
          phiQueue_.push(phi);
        } else {
          queue_.push(userInst);
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void TypeAnalysis::Solve()
{
  // Record all argument instructions for later lookup.
  for (auto &func : prog_) {
    for (auto &block : func) {
      for (auto &inst : block) {
        if (auto *arg = ::cast_or_null<const ArgInst>(&inst)) {
          args_[std::make_pair(&func, arg->GetIndex())].push_back(arg);
        }
      }
    }
  }
  // Over-approximate all arguments to exported or indirectly reachable
  // functions to the most generic type. Use these values to seed the analysis.
  for (Func &func : prog_) {
    for (auto &block : func) {
      for (auto &inst : block) {
        Init(*this, target_).Dispatch(inst);
      }
    }
  }
  // Propagate types through the queued instructions.
  while (!queue_.empty() || !phiQueue_.empty()) {
    while (!queue_.empty()) {
      auto *inst = queue_.front();
      inQueue_.erase(inst);
      queue_.pop();
      Step(*this, target_).Dispatch(*inst);
    }
    while (queue_.empty() && !phiQueue_.empty()) {
      auto *inst = phiQueue_.front();
      inQueue_.erase(inst);
      phiQueue_.pop();
      Step(*this, target_).Dispatch(*inst);
    }
  }
}

// -----------------------------------------------------------------------------
void TypeAnalysis::dump(llvm::raw_ostream &os)
{
  class AnalysisPrinter : public Printer {
  public:
    AnalysisPrinter(llvm::raw_ostream &os, TypeAnalysis &that)
      : Printer(os)
      , that_(that)
    {
    }

    void PrintFuncHeader(const Func &func)
    {
      os_ << "\t.eliminate-select:type ";

      for (unsigned i = 0, n = func.params().size(); i < n; ++i) {
        if (i != 0) {
          os_ << ", ";
        }
        if (auto it = that_.args_.find({ &func, i }); it != that_.args_.end()) {
          if (!it->second.empty()) {
            os_ << that_.Find(it->second[0]);
          }
        }
      }

      os_ << " -> ";

      if (auto it = that_.rets_.find(&func); it != that_.rets_.end()) {
        for (unsigned i = 0, n = it->second.size(); i < n; ++i) {
          if (i != 0) {
            os_ << ", ";
          }
          os_ << it->second[i];
        }
      }

      os_ << "\n";
    }

    void PrintInstHeader(const Inst &inst)
    {
      std::string str;
      llvm::raw_string_ostream os(str);
      for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
        if (i != 0) {
          os << ", ";
        }
        os << that_.Find(inst.GetSubValue(i));
      }
      os_ << str;
      for (unsigned i = str.size(); i < 30; ++i) {
        os_ << ' ';
      }
    }

  private:
    /// Reference to the analysis.
    TypeAnalysis &that_;
  };

  AnalysisPrinter(os, *this).Print(prog_);
}
