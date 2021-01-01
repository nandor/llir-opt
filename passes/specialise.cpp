// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/specialise.h"



// -----------------------------------------------------------------------------
const char *SpecialisePass::kPassID = "specialise";

// -----------------------------------------------------------------------------
bool SpecialisePass::Run(Prog &prog)
{
  // Identify simple higher order functions - those which invoke an argument.
  std::unordered_map<Func *, llvm::DenseSet<unsigned>> higherOrderFuncs;
  for (auto &func : prog) {
    // Find arguments which reach a call site.
    std::vector<Ref<ArgInst>> args;
    for (auto &block : func) {
      for (auto &inst : block) {
        if (auto *call = ::cast_or_null<CallSite>(&inst)) {
          Ref<Inst> calleeRef = call->GetCallee();
          if (Ref<ArgInst> argRef = ::cast_or_null<ArgInst>(calleeRef)) {
            args.push_back(argRef);
          }
        }
      }
    }

    if (args.empty()) {
      continue;
    }

    // Arguments should only be invoked, they should not escape.
    bool escapes = false;
    llvm::DenseSet<unsigned> indices;
    for (Ref<ArgInst> argRef: args) {
      ArgInst *arg = argRef.Get();
      for (auto *user : arg->users()) {
        if (auto *call = ::cast_or_null<CallSite>(user)) {
          if (call->GetCallee() != argRef) {
            escapes = true;
            break;
          }
        } else {
          escapes = true;
          break;
        }
      }
      if (escapes) {
        break;
      }
      indices.insert(arg->GetIndex());
    }

    // Candidate function, along with HO arguments.
    if (!escapes) {
      higherOrderFuncs.emplace(&func, std::move(indices));
    }
  }

  // Find the call sites of these HOFs, identifying arguments.
  std::map<Func *, std::map<Params, std::set<CallSite *>>> sites;
  std::map<Func *, unsigned> uses;
  for (Func &func : prog) {
    for (Block &block : func) {
      auto *call = ::cast_or_null<CallSite>(block.GetTerminator());
      if (!call) {
        continue;
      }
      auto *func = call->GetDirectCallee();
      if (!func) {
        continue;
      }
      uses[func]++;
      auto it = higherOrderFuncs.find(func);
      if (it == higherOrderFuncs.end()) {
        continue;
      }

      // Check for function arguments.
      Params params;
      bool specialise = true;
      for (unsigned i : it->second) {
        if (i < call->arg_size()) {
          if (auto instRef = ::cast_or_null<MovInst>(call->arg(i))) {
            if (auto funcRef = ::cast_or_null<Func>(instRef->GetArg())) {
              params.emplace_back(i, funcRef.Get());
              continue;
            }
          }
        }
        specialise = false;
        break;
      }

      // Record the specialisation site.
      if (specialise) {
        sites[func][params].insert(call);
      }
    }
  }

  // Check if the function is worth specialising and specialise it.
  bool changed = false;
  for (const auto &[func, sites] : sites) {
    // Specialise only if all uses of the HOF are explicit.
    unsigned count = 0;
    for (auto &[params, calls] : sites) {
      count += calls.size();
    }
    if (count != uses[func]) {
      continue;
    }

    for (auto &[params, calls] : sites) {
      // Create a new instance of the function with the given parameters.
      Func *specialised = Specialise(func, params);

      // Modify the call sites.
      for (auto *inst : calls) {
        Block *parent = inst->getParent();
        // Specialise the arguments, replacing some with values.
        const auto &[args, flags] = Specialise(inst, params);

        // Create a mov which takes the address of the function.
        auto *newMov = new MovInst(Type::I64, specialised, {});
        parent->AddInst(newMov, inst);

        // Compute the new number of arguments.
        std::optional<unsigned> numArgs;
        if (auto fixed = inst->GetNumFixedArgs()) {
          numArgs = *fixed - inst->arg_size() + args.size();
        }

        // Replace the old call with one to newMov.
        Inst *newCall = nullptr;
        switch (inst->GetKind()) {
          case Inst::Kind::CALL: {
            auto *call = static_cast<CallInst *>(inst);
            newCall = new CallInst(
                call->GetTypes(),
                newMov,
                args,
                flags,
                call->GetCont(),
                numArgs,
                call->GetCallingConv(),
                call->GetAnnots()
            );
            break;
          }
          case Inst::Kind::INVOKE: {
            auto *call = static_cast<InvokeInst *>(inst);
            newCall = new InvokeInst(
                call->GetTypes(),
                newMov,
                args,
                flags,
                call->GetCont(),
                call->GetThrow(),
                numArgs,
                call->GetCallingConv(),
                call->GetAnnots()
            );
            break;
          }
          case Inst::Kind::TAIL_CALL: {
            auto *call = static_cast<TailCallInst *>(inst);
            newCall = new TailCallInst(
                call->GetTypes(),
                newMov,
                args,
                flags,
                numArgs,
                call->GetCallingConv(),
                call->GetAnnots()
            );
            break;
          }
          default: {
            llvm_unreachable("invalid instruction");
          }
        }
        parent->AddInst(newCall, inst);
        inst->replaceAllUsesWith(newCall);
        inst->eraseFromParent();
        changed = true;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
class SpecialiseClone final : public CloneVisitor {
public:
  /// Prepare the clone context.
  SpecialiseClone(
      Func *oldFunc,
      Func *newFunc,
      const llvm::DenseMap<unsigned, Ref<Func>> &funcs,
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
  Ref<Inst> Map(Ref<Inst> inst) override
  {
    auto [it, inserted] = insts_.emplace(inst.Get(), nullptr);
    if (inserted) {
      it->second = CloneVisitor::Clone(it->first);
    }
    return Ref(it->second, inst.Index());
  }

  /// Clones an argument inst, substituting the actual value.
  Inst *Clone(ArgInst *i) override
  {
    const auto &annot = i->GetAnnots();
    if (auto it = funcs_.find(i->GetIndex()); it != funcs_.end()) {
      return new MovInst(Type::I64, it->second, annot);
    } else if (auto it = args_.find(i->GetIndex()); it != args_.end()) {
      Type type = newFunc_->params()[it->second].GetType();
      return new ArgInst(type, it->second, annot);
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
  const llvm::DenseMap<unsigned, Ref<Func>> &funcs_;
  /// Mapping from old argument indices to new ones.
  const llvm::DenseMap<unsigned, unsigned> &args_;

  /// Map of cloned blocks.
  std::unordered_map<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
};

// -----------------------------------------------------------------------------
Func *SpecialisePass::Specialise(Func *oldFunc, const Params &params)
{
  llvm::DenseMap<unsigned, Ref<Func>> funcs;

  // Compute the function name and a mapping for args from the parameters.
  std::ostringstream os;
  os << oldFunc->GetName();
  for (auto &param : params) {
    funcs.insert({param.first, param.second});
    os << "$" << param.second->GetName();
  }

  // Find the type of the new function.
  llvm::DenseMap<unsigned, unsigned> args;
  std::vector<FlaggedType> types;
  unsigned i = 0, index = 0;
  for (const FlaggedType arg : oldFunc->params()) {
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
        newBlock->AddInst(clone.Map(&oldInst).Get());
      }
      newFunc->AddBlock(newBlock);
    }
  }

  return newFunc;
}

// -----------------------------------------------------------------------------
std::pair<std::vector<Ref<Inst>>, std::vector<TypeFlag>>
SpecialisePass::Specialise(CallSite *call, const Params &params)
{
  std::vector<Ref<Inst>> args;
  std::vector<TypeFlag> flags;
  for (unsigned i = 0, n = call->arg_size(); i < n; ++i) {
    bool replace = false;
    for (auto &param : params) {
      if (param.first == i) {
        replace = true;
        break;
      }
    }
    if (!replace) {
      args.push_back(call->arg(i));
      flags.push_back(call->GetFlag(i));
    }
  }
  return { args, flags };
}

// -----------------------------------------------------------------------------
const char *SpecialisePass::GetPassName() const
{
  return "Higher Order Specialisation";
}
