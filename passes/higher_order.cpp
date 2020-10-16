// This file if part of the llir-opt project.
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
const char *HigherOrderPass::kPassID = "higher-order";

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
        if (Inst *callee = GetCalledInst(&inst)) {
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
          Inst *callee = GetCalledInst(inst);
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
              case Inst::Kind::CALL:
              case Inst::Kind::INVOKE:
              case Inst::Kind::TCALL: {
                auto *call = static_cast<CallInst *>(inst);
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
        auto *newMov = new MovInst(Type::I64, specialised, {});
        parent->AddInst(newMov, inst);

        // Replace the old call with one to newMov.
        Inst *newCall = nullptr;
        switch (inst->GetKind()) {
          case Inst::Kind::CALL: {
            auto *call = static_cast<CallInst *>(inst);
            std::vector<Inst *> args = Specialise(call, params);
            newCall = new CallInst(
                call->GetType(),
                newMov,
                args,
                call->GetCont(),
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnots()
            );
            break;
          }
          case Inst::Kind::INVOKE: {
            auto *call = static_cast<InvokeInst *>(inst);
            std::vector<Inst *> args = Specialise(call, params);
            newCall = new InvokeInst(
                call->GetType(),
                newMov,
                args,
                call->GetCont(),
                call->GetThrow(),
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnots()
            );
            break;
          }
          case Inst::Kind::TCALL: {
            auto *call = static_cast<TailCallInst *>(inst);
            std::vector<Inst *> args = Specialise(call, params);
            newCall = new TailCallInst(
                call->GetType(),
                newMov,
                args,
                call->GetNumFixedArgs() - call->GetNumArgs() + args.size(),
                call->GetCallingConv(),
                call->GetAnnots()
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
class SpecialiseClone final : public CloneVisitor {
public:
  /// Prepare the clone context.
  SpecialiseClone(
      Func *oldFunc,
      Func *newFunc,
      const llvm::DenseMap<unsigned, Func *> &funcs,
      const llvm::DenseMap<unsigned, unsigned> &args)
    : oldFunc_(oldFunc)
    , newFunc_(newFunc)
    , funcs_(funcs)
    , args_(args)
  {
  }

  /// Apply fixups.
  ~SpecialiseClone()
  {
    Fixup();
  }

  /// Maps a block.
  Block *Map(Block *block) override
  {
    if (auto [it, inserted] = blocks_.emplace(block, nullptr); inserted) {
      static unsigned uniqueID = 0;
      std::ostringstream os;
      os << block->GetName() << "$hof$" << newFunc_->GetName() << uniqueID++;
      it->second = new Block(os.str());
      return it->second;
    } else {
      return it->second;
    }
  }

  /// Maps an instruction.
  Inst *Map(Inst *inst) override
  {
    if (auto [it, inserted] = insts_.emplace(inst, nullptr); inserted) {
      it->second = CloneVisitor::Clone(inst);
      return it->second;
    } else {
      return it->second;
    }
  }

  /// Clones an argument inst, substituting the actual value.
  Inst *Clone(ArgInst *i) override
  {
    const auto &annot = i->GetAnnots();
    if (auto it = funcs_.find(i->GetIdx()); it != funcs_.end()) {
      return new MovInst(Type::I64, it->second, annot);
    } else if (auto it = args_.find(i->GetIdx()); it != args_.end()) {
      Type type = oldFunc_->params()[it->second];
      return new ArgInst(type, new ConstantInt(it->second), annot);
    } else {
      llvm_unreachable("Argument out of range");
    }
  }

private:
  /// Old function.
  Func *oldFunc_;
  /// New function.
  Func *newFunc_;
  /// Mapping from argument indices to arguments.
  const llvm::DenseMap<unsigned, Func *> &funcs_;
  /// Mapping from old argument indices to new ones.
  const llvm::DenseMap<unsigned, unsigned> &args_;

  /// Map of cloned blocks.
  std::unordered_map<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
};

// -----------------------------------------------------------------------------
Func *HigherOrderPass::Specialise(Func *oldFunc, const Params &params)
{
  llvm::DenseMap<unsigned, Func *> funcs;

  // Compute the function name and a mapping for args from the parameters.
  std::ostringstream os;
  os << oldFunc->GetName();
  for (auto &param : params) {
    funcs.insert({param.first, param.second});
    os << "$" << param.second->GetName();
  }

  // Find the type of the new function.
  llvm::DenseMap<unsigned, unsigned> args;
  std::vector<Type> types;
  unsigned i = 0, index = 0;
  for (const Type arg : oldFunc->params()) {
    if (funcs.find(i) == funcs.end()) {
      args.insert({ i, index });
      types.push_back(arg);
      ++index;
    }
    ++i;
  }

  // Create a function and add it to the program.
  Func *newFunc = new Func(os.str());
  newFunc->SetCallingConv(oldFunc->GetCallingConv());
  newFunc->SetVarArg(oldFunc->IsVarArg());
  newFunc->SetParameters(types);
  newFunc->SetVisibility(Visibility::LOCAL);
  for (auto &object : oldFunc->objects()) {
    newFunc->AddStackObject(object.Index, object.Size, object.Alignment);
  }
  oldFunc->getParent()->AddFunc(newFunc, oldFunc);

  // Clone all blocks.
  {
    SpecialiseClone clone(oldFunc, newFunc, funcs, args);
    for (auto &oldBlock : *oldFunc) {
      auto *newBlock = clone.Map(&oldBlock);
      for (auto &oldInst : oldBlock) {
        newBlock->AddInst(clone.Map(&oldInst));
      }
      newFunc->AddBlock(newBlock);
    }
  }

  return newFunc;
}

// -----------------------------------------------------------------------------
std::vector<Inst *>
HigherOrderPass::Specialise(CallSite *call, const Params &params)
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
