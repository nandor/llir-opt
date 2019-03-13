// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <map>
#include <set>
#include <sstream>

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/higher_order.h"



// -----------------------------------------------------------------------------
static Inst *GetCallee(Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      return static_cast<CallInst *>(inst)->GetCallee();
    }
    case Inst::Kind::INVOKE: {
      return static_cast<InvokeInst *>(inst)->GetCallee();
    }
    case Inst::Kind::TCALL: {
      return static_cast<TailCallInst *>(inst)->GetCallee();
    }
    case Inst::Kind::TINVOKE: {
      return static_cast<TailInvokeInst *>(inst)->GetCallee();
    }
    default: {
      return nullptr;
    }
  }
}

// -----------------------------------------------------------------------------
void HigherOrderPass::Run(Prog *prog)
{
  // Identify simple higher order functions - those which invoke an argument.
  std::unordered_map<Func *, llvm::DenseSet<ArgInst *>> higherOrderFuncs;
  for (auto &func : *prog) {
    // Find arguments which reach a call site.
    llvm::DenseSet<ArgInst *> args;
    for (auto &block : func) {
      for (auto &inst : block) {
        if (Inst *callee = GetCallee(&inst)) {
          if (callee->Is(Inst::Kind::ARG)) {
            args.insert(static_cast<ArgInst *>(callee));
          }
        }
      }
    }

    if (args.empty()) {
      continue;
    }

    // Arguments should only be invoked, they should not escape.
    bool escapes = false;
    for (auto *arg : args) {
      for (auto *user : arg->users()) {
        if (auto *inst = ::dyn_cast_or_null<Inst>(user)) {
          Inst *callee = GetCallee(inst);
          if (callee != arg) {
            escapes = true;
            break;
          }
        } else {
          escapes = true;
          break;
        }
      }
    }

    // Candidate function, along with HO arguments.
    if (!escapes) {
      higherOrderFuncs.emplace(&func, std::move(args));
    }
  }

  // Find the call sites of these HOFs, identifying arguments.
  std::map<std::pair<Func *, Params>, std::set<Inst *>> sites;
  for (const auto &[func, args] : higherOrderFuncs) {
    for (auto *funcUser : func->users()) {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(funcUser)) {
        for (auto *movUser : movInst->users()) {
          if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
            // Find the arguments to the call of the higher-order function.
            std::vector<Inst *> actualArgs;
            switch (inst->GetKind()) {
              case Inst::Kind::CALL: {
                auto *call = static_cast<CallInst *>(inst);
                if (call->GetCallee() == movInst) {
                  std::copy(
                      call->arg_begin(), call->arg_end(),
                      std::back_inserter(actualArgs)
                  );
                }
                break;
              }
              case Inst::Kind::INVOKE:
              case Inst::Kind::TCALL:
              case Inst::Kind::TINVOKE: {
                auto *call = static_cast<CallSite<TerminatorInst> *>(inst);
                if (call->GetCallee() == movInst) {
                  std::copy(
                      call->arg_begin(), call->arg_end(),
                      std::back_inserter(actualArgs)
                  );
                }
                break;
              }
              default: {
                // Nothing to specialise here.
                break;
              }
            }

            // Check for function arguments.
            bool specialise = !actualArgs.empty();
            Params params;
            for (ArgInst *arg : args) {
              const unsigned i = arg->GetIdx();
              if (i < actualArgs.size()) {
                if (auto *inst = ::dyn_cast_or_null<MovInst>(actualArgs[i])) {
                  if (auto *func = ::dyn_cast_or_null<Func>(inst->GetArg())) {
                    params.emplace_back(i, func);
                    continue;
                  }
                }
              }

              specialise = false;
              break;
            }

            // Record the specialisation site.
            if (specialise) {
              sites.emplace(
                  std::make_pair(func, params),
                  std::set<Inst *>{}
              ).first->second.insert(inst);
            }
          }
        }
      }
    }
  }

  // Check if the function is worth specialising and specialise it.
  for (const auto &[key, insts] : sites) {
    const auto &[func, params] = key;

    // Decide if the function should be specialised.
    bool specialise = true;
    if (params.size() == 1) {
      auto *param = params[0].second;

      // Only specialise if all the uses of the func are among the sites.
      for (auto *funcUser : param->users()) {
        if (auto *movInst = ::dyn_cast_or_null<MovInst>(funcUser)) {
          bool valid = true;
          for (auto *movUser : movInst->users()) {
            if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
              if (insts.count(inst) != 0) {
                continue;
              }
            }
            valid = false;
            break;
          }

          if (valid) {
            continue;
          }
        }

        specialise = false;
        break;
      }
    }

    if (specialise) {
      // Create a new instance of the function with the given parameters.
      Func *specialised = Specialise(func, params);

      // Modify the call sites.
      for (auto *inst : insts) {
        Block *parent = inst->getParent();

        // Create a mov which takes the address of the function.
        auto *newMov = new MovInst(Type::I64, specialised);
        parent->AddInst(newMov, inst);

        // Replace the old call with one to newMov.
        Inst *newCall = nullptr;
        switch (inst->GetKind()) {
          case Inst::Kind::CALL: {
            auto *call = static_cast<CallInst *>(inst);
            std::vector<Inst *> args = Specialise<CallInst>(call, params);
            newCall = new CallInst(
                call->GetType(),
                newMov,
                args,
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnotation()
            );
            break;
          }
          case Inst::Kind::INVOKE: {
            auto *call = static_cast<InvokeInst *>(inst);
            std::vector<Inst *> args = Specialise<InvokeInst>(call, params);
            newCall = new InvokeInst(
                call->GetType(),
                newMov,
                args,
                call->GetCont(),
                call->GetThrow(),
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnotation()
            );
            break;
          }
          case Inst::Kind::TCALL: {
            auto *call = static_cast<TailCallInst *>(inst);
            std::vector<Inst *> args = Specialise<TailCallInst>(call, params);
            newCall = new TailCallInst(
                call->GetType(),
                newMov,
                args,
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnotation()
            );
            break;
          }
          case Inst::Kind::TINVOKE: {
            auto *call = static_cast<TailInvokeInst *>(inst);
            std::vector<Inst *> args = Specialise<TailInvokeInst>(call, params);
            newCall = new TailInvokeInst(
                call->GetType(),
                newMov,
                args,
                call->GetThrow(),
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnotation()
            );
            break;
          }
          default: {
            assert(!"invalid instruction");
            continue;
          }
        }
        parent->AddInst(newCall, inst);
        inst->replaceAllUsesWith(newCall);
        inst->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
class SpecialiseClone final : public InstClone {
public:
  SpecialiseClone(
      Func *oldFunc,
      Func *newFunc,
      const std::vector<std::pair<unsigned, Func *>> &params)
    : oldFunc_(oldFunc)
    , newFunc_(newFunc)
  {
    for (auto &param : params) {
      params_.emplace(param.first, param.second);
    }
  }

  /// Clones a block.
  Block *Make(Block *block) override
  {
    std::ostringstream os;
    os << newFunc_->GetName() << block->GetName();
    return new Block(os.str());
  }

  /// Clones an argument inst, substituting the actual value.
  Inst *Make(ArgInst *i) override
  {
    if (auto it = params_.find(i->GetIdx()); it != params_.end()) {
      return new MovInst(Type::I64, it->second);
    } else {
      return InstClone::Make(i);
    }
  }

private:
  /// Old function.
  Func *oldFunc_;
  /// New function.
  Func *newFunc_;
  /// Mapping from argument indices to arguments.
  std::map<unsigned, Func *> params_;
};

// -----------------------------------------------------------------------------
Func *HigherOrderPass::Specialise(Func *oldFunc, const Params &params)
{
  std::ostringstream os;
  os << oldFunc->GetName();
  for (auto &param : params) {
    os << "$" << param.second->GetName();
  }

  Prog *prog = oldFunc->getParent();
  Func *newFunc = new Func(oldFunc->getParent(), os.str());
  prog->AddFunc(newFunc, oldFunc);

  // Clone all blocks.
  {
    SpecialiseClone clone(oldFunc, newFunc, params);
    for (auto &oldBlock : *oldFunc) {
      auto *newBlock = clone.Clone(&oldBlock);
      for (auto &oldInst : oldBlock) {
        newBlock->AddInst(clone.Clone(&oldInst));
      }
      newFunc->AddBlock(newBlock);
    }
  }

  return newFunc;
}

// -----------------------------------------------------------------------------
template<typename T>
std::vector<Inst *> HigherOrderPass::Specialise(T *call, const Params &params)
{
  unsigned i = 0;
  std::vector<Inst *> args;
  for (auto it = call->arg_begin(), e = call->arg_end(); it != e; ++it, ++i) {
    bool replace = false;
    for (auto &param : params) {
      if (param.first == i) {
        replace = true;
        break;
      }
    }
    if (!replace) {
      args.push_back(*it);
    }
  }
  return args;
}

// -----------------------------------------------------------------------------
const char *HigherOrderPass::GetPassName() const
{
  return "Higher Order Specialisation";
}
