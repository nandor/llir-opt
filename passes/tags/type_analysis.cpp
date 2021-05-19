// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Format.h>

#include "core/prog.h"
#include "core/printer.h"
#include "passes/tags/init.h"
#include "passes/tags/step.h"
#include "passes/tags/refinement.h"
#include "passes/tags/type_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
static bool Converges(Type ty, TaggedType told, TaggedType tnew)
{
  return (told < tnew) && (ty != Type::V64 || tnew <= TaggedType::Val());
}

// -----------------------------------------------------------------------------
bool TypeAnalysis::Mark(Ref<Inst> inst, const TaggedType &tnew)
{
  auto it = types_.emplace(inst, tnew);
  if (it.second) {
    Enqueue(inst);
    return true;
  } else {
    auto told = it.first->second;
    if (told == tnew) {
      return false;
    } else {
      #ifndef NDEBUG
      if (!Converges(inst.GetType(), told, tnew)) {
        std::string msg;
        llvm::raw_string_ostream os(msg);
        os << "no convergence:\n";
        os << told << " " << tnew << "\n";
        os << inst->getParent()->getName() << "\n";
        os << *inst << "\n";
        llvm::report_fatal_error(msg.c_str());
      }
      #endif
      it.first->second = tnew;
      Enqueue(inst);
      return true;
    }
  }
}

// -----------------------------------------------------------------------------
bool TypeAnalysis::Refine(Ref<Inst> inst, const TaggedType &tnew)
{
  auto it = types_.find(inst);
  assert(it != types_.end() && "no type to override");
  auto told = it->second;
  if (tnew < told) {
    it->second = tnew;
    for (Use &use : inst->uses()) {
      if (use.get() == inst) {
        auto *userInst = ::cast<Inst>(use.getUser());
        if (inRefineQueue_.insert(userInst).second) {
          refineQueue_.push(userInst);
        }
      }
    }
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
void TypeAnalysis::Enqueue(Ref<Inst> inst)
{
  Func *f = const_cast<Func *>(inst->getParent()->getParent());
  if (inBackwardQueue_.insert(f).second) {
    backwardQueue_.push(f);
  }

  for (Use &use : inst->uses()) {
    if (use.get() == inst) {
      auto *userInst = ::cast<Inst>(use.getUser());
      if (inForwardQueue_.insert(userInst).second) {
        if (auto *phi = ::cast_or_null<PhiInst>(userInst)) {
          forwardPhiQueue_.push(phi);
        } else {
          forwardQueue_.push(userInst);
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
        if (auto *arg = ::cast_or_null<ArgInst>(&inst)) {
          args_[std::make_pair(&func, arg->GetIndex())].push_back(arg);
        }
      }
    }
  }
  // Over-approximate all arguments to exported or indirectly reachable
  // functions to the most generic type. Use these values to seed the analysis.
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        Init(*this, target_).Dispatch(inst);
      }
    }
  }
  // Propagate types through the queued instructions.
  while (!forwardQueue_.empty() || !forwardPhiQueue_.empty()) {
    while (!forwardQueue_.empty()) {
      auto *inst = forwardQueue_.front();
      inForwardQueue_.erase(inst);
      forwardQueue_.pop();
      Step(*this, target_, Step::Kind::FORWARD).Dispatch(*inst);
    }
    while (forwardQueue_.empty() && !forwardPhiQueue_.empty()) {
      auto *inst = forwardPhiQueue_.front();
      inForwardQueue_.erase(inst);
      forwardPhiQueue_.pop();
      Step(*this, target_, Step::Kind::FORWARD).Dispatch(*inst);
    }
  }
  // Propagate types through the queued instructions.
  while (!refineQueue_.empty() || !backwardQueue_.empty()) {
    while (!backwardQueue_.empty()) {
      auto *f = backwardQueue_.front();
      inBackwardQueue_.erase(f);
      backwardQueue_.pop();
      Refinement(*this, target_, *f).Run();
    }
    while (!refineQueue_.empty()) {
      auto *inst = refineQueue_.front();
      inRefineQueue_.erase(inst);
      refineQueue_.pop();
      Step(*this, target_, Step::Kind::REFINE).Dispatch(*inst);
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
        std::pair<const Func *, unsigned> key{ &func, i };
        if (auto it = that_.args_.find(key); it != that_.args_.end()) {
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
