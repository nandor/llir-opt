// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <map>
#include <sstream>
#include <unordered_set>

#include "core/adt/hash.h"
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
const char *SpecialisePass::GetPassName() const
{
  return "Higher Order Specialisation";
}

// -----------------------------------------------------------------------------
bool SpecialisePass::Run(Prog &prog)
{
  /// Mapping from parameters to call sites.
  using SiteMap = std::unordered_map
      < Parameters
      , std::set<CallSite *>
      , ParametersHash
      >;

  // Find the call sites with constant arguments.
  std::unordered_map<Func *, SiteMap> funcCallSites;
  std::unordered_map<Func *, unsigned> uses;
  for (Func &caller : prog) {
    for (Block &block : caller) {
      auto *call = ::cast_or_null<CallSite>(block.GetTerminator());
      if (!call) {
        continue;
      }
      auto *func = call->GetDirectCallee();
      if (!func) {
        continue;
      }
      if (func->HasAddressTaken() || !func->IsLocal() || func->IsNoInline()) {
        continue;
      }
      if (func->getName().contains("$specialised$")) {
        continue;
      }
      uses[func]++;

      // Check for function arguments.
      Parameters params;
      for (unsigned i = 0, n = call->arg_size(); i < n; ++i) {
        if (auto instRef = ::cast_or_null<MovInst>(call->arg(i))) {
          auto v = instRef->GetArg();
          switch (v->GetKind()) {
            case Value::Kind::INST: {
              continue;
            }
            case Value::Kind::GLOBAL: {
              params.emplace(
                  std::piecewise_construct,
                  std::forward_as_tuple(i),
                  std::forward_as_tuple(&*::cast<Global>(v), 0)
              );
              continue;
            }
            case Value::Kind::EXPR: {
              switch (::cast<Expr>(v)->GetKind()) {
                case Expr::Kind::SYMBOL_OFFSET: {
                  auto s = ::cast<SymbolOffsetExpr>(v);
                  params.emplace(
                      std::piecewise_construct,
                      std::forward_as_tuple(i),
                      std::forward_as_tuple(s->GetSymbol(), s->GetOffset())
                  );
                  continue;
                }
              }
              llvm_unreachable("not implemented");
            }
            case Value::Kind::CONST: {
              switch (::cast<Constant>(v)->GetKind()) {
                case Constant::Kind::INT: {
                  params.emplace(
                      std::piecewise_construct,
                      std::forward_as_tuple(i),
                      std::forward_as_tuple(::cast<ConstantInt>(v)->GetValue())
                  );
                  continue;
                }
                case Constant::Kind::FLOAT: {
                  params.emplace(
                      std::piecewise_construct,
                      std::forward_as_tuple(i),
                      std::forward_as_tuple(::cast<ConstantFloat>(v)->GetValue())
                  );
                  continue;
                }
              }
              llvm_unreachable("invalid constant kind");
            }
          }
          llvm_unreachable("invalid value kind");
        }
      }

      // Record the specialisation site.
      if (!params.empty()) {
        funcCallSites[func][params].insert(call);
      }
    }
  }

  bool changed = false;
  // In the first round, specialise all functions which are always
  // invoked with the same sets of constant arguments.
  for (auto it = funcCallSites.begin(); it != funcCallSites.end(); ) {
    auto &[func, sites] = *it;

    if (sites.size() == 1 && uses[func] == sites.begin()->second.size()) {
      auto &[params, calls] = *sites.begin();
      Specialise(func, params, calls);
      funcCallSites.erase(it++);
      changed = true;
    } else {
      ++it;
    }
  }
  // In the second round, filter out non-call parameters and specialise all.
  for (auto it = funcCallSites.begin(); it != funcCallSites.end(); ) {
    SiteMap indirectMap;

    auto &[func, specs] = *it;
    for (auto st = specs.begin(); st != specs.end(); ) {
      Parameters params;
      for (auto &[i, param] : st->first) {
        switch (param.K) {
          case Parameter::Kind::INT:
          case Parameter::Kind::FLOAT: {
            continue;
          }
          case Parameter::Kind::GLOBAL: {
            if (param.GlobalVal.Symbol->Is(Global::Kind::FUNC)) {
              params.emplace(i, param);
            }
            continue;
          }
        }
        llvm_unreachable("invalid parameter kind");
      }

      if (!params.empty()) {
        for (auto *site : st->second) {
          indirectMap[params].insert(site);
        }
        specs.erase(st++);
      } else {
        ++st;
      }
    }

    for (auto &[params, calls] : indirectMap) {
      Specialise(func, params, calls);
      changed = true;
    }

    if (specs.empty()) {
      funcCallSites.erase(it++);
    } else {
      ++it;
    }
  }
  // Last round - specialise a single argument.
  for (auto &[func, specs] : funcCallSites) {
    SiteMap singleMap;
    for (auto &[params, sites] : specs) {
      for (auto &[i, param] : params) {
        Parameters key;
        key.emplace(i, param);
        for (auto *site : sites) {
          singleMap[key].insert(site);
        }
      }
    }

    std::vector<std::pair<Parameters, std::set<CallSite *>>> ordered;
    for (auto &[key, sites] : singleMap) {
      ordered.emplace_back(key, sites);
    }
    std::sort(ordered.begin(), ordered.end(), [] (auto &lhs, auto &rhs) {
      return lhs.second.size() > rhs.second.size();
    });

    if (!ordered.empty()) {
      auto &[params, sites] = ordered[0];
      unsigned ns = sites.size();
      unsigned nf = uses[func];
      if (ns == nf || (ns * 2 >= nf && func->inst_size() < 15)) {
        Specialise(func, params, sites);
        changed = true;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
class SpecialisePass::SpecialiseClone final : public CloneVisitor {
public:
  /// Prepare the clone context.
  SpecialiseClone(
      Func *oldFunc,
      Func *newFunc,
      const llvm::DenseMap<unsigned, Parameter> &values,
      const llvm::DenseMap<unsigned, unsigned> &args)
    : oldFunc_(oldFunc)
    , newFunc_(newFunc)
    , values_(values)
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
    if (auto it = values_.find(i->GetIndex()); it != values_.end()) {
      return new MovInst(i->GetType(), it->second.ToValue(), annot);
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
  const llvm::DenseMap<unsigned, Parameter> &values_;
  /// Mapping from old argument indices to new ones.
  const llvm::DenseMap<unsigned, unsigned> &args_;

  /// Map of cloned blocks.
  std::unordered_map<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
};

// -----------------------------------------------------------------------------
void SpecialisePass::Specialise(
    Func *func,
    const Parameters &params,
    const std::set<CallSite *> &callSites)
{
  Func *specialised = Specialise(func, params);
  for (auto *inst : callSites) {
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
  }
}

// -----------------------------------------------------------------------------
Func *SpecialisePass::Specialise(Func *oldFunc, const Parameters &params)
{
  llvm::DenseMap<unsigned, Parameter> argValues;

  // Compute the function name and a mapping for args from the parameters.
  std::string name;
  llvm::raw_string_ostream os(name);
  os << oldFunc->getName() << "$specialised";
  for (auto &[idx, v] : params) {
    argValues.insert({idx, v});
    switch (v.K) {
      case Parameter::Kind::INT: {
        os << "$" << v.IntVal;
        continue;
      }
      case Parameter::Kind::FLOAT: {
        llvm::SmallVector<char, 16> buffer;
        v.FloatVal.toString(buffer);
        os << "$" << buffer;
        continue;
      }
      case Parameter::Kind::GLOBAL: {
        auto &g = v.GlobalVal;
        os << "$" << g.Symbol->getName() << "_" << g.Offset;
        continue;
      }
      llvm_unreachable("invalid value kind");
    }
  }

  // Find the type of the new function.
  llvm::DenseMap<unsigned, unsigned> args;
  std::vector<FlaggedType> types;
  unsigned i = 0, index = 0;
  for (const FlaggedType arg : oldFunc->params()) {
    if (argValues.find(i) == argValues.end()) {
      args.insert({ i, index });
      types.push_back(arg);
      ++index;
    }
    ++i;
  }

  // Create a function and add it to the program.
  Func *newFunc = new Func(name);
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
    SpecialiseClone clone(oldFunc, newFunc, argValues, args);
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
SpecialisePass::Specialise(CallSite *call, const Parameters &params)
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
SpecialisePass::Parameter::Parameter(const Parameter &that)
  : K(that.K)
{
  switch (K) {
    case Parameter::Kind::INT: new (&IntVal) APInt(that.IntVal); break;
    case Parameter::Kind::FLOAT: new (&FloatVal) APFloat(that.FloatVal); break;
    case Parameter::Kind::GLOBAL: new (&GlobalVal) G(that.GlobalVal); break;
  }
}

// -----------------------------------------------------------------------------
SpecialisePass::Parameter::~Parameter()
{
  switch (K) {
    case Parameter::Kind::INT: IntVal.~APInt(); return;
    case Parameter::Kind::FLOAT: FloatVal.~APFloat(); return;
    case Parameter::Kind::GLOBAL: return;
  }
  llvm_unreachable("invalid parameter kind");
}

// -----------------------------------------------------------------------------
bool SpecialisePass::Parameter::operator==(const Parameter &that) const
{
  if (K != that.K) {
    return false;
  }
  switch (K) {
    case Parameter::Kind::INT: {
      if (IntVal.getBitWidth() != that.IntVal.getBitWidth()) {
        return false;
      }
      return IntVal == that.IntVal;
    }
    case Parameter::Kind::FLOAT: {
      if (&FloatVal.getSemantics() != &that.FloatVal.getSemantics()) {
        return false;
      }
      return FloatVal == that.FloatVal;
    }
    case Parameter::Kind::GLOBAL: {
      return GlobalVal == that.GlobalVal;
    }
  }
  llvm_unreachable("invalid parameter kind");
}

// -----------------------------------------------------------------------------
SpecialisePass::Parameter &
SpecialisePass::Parameter::operator=(const Parameter &that)
{
  switch (K) {
    case Parameter::Kind::INT: IntVal.~APInt(); break;
    case Parameter::Kind::FLOAT: FloatVal.~APFloat(); break;
    case Parameter::Kind::GLOBAL: break;
  }
  K = that.K;
  switch (K) {
    case Parameter::Kind::INT: new (&IntVal) APInt(that.IntVal); break;
    case Parameter::Kind::FLOAT: new (&FloatVal) APFloat(that.FloatVal); break;
    case Parameter::Kind::GLOBAL: new (&GlobalVal) G(that.GlobalVal); break;
  }
  return *this;
}

// -----------------------------------------------------------------------------
Value *SpecialisePass::Parameter::ToValue() const
{
  switch (K) {
    case Parameter::Kind::INT: {
      return new ConstantInt(IntVal);
    }
    case Parameter::Kind::FLOAT: {
      return new ConstantFloat(FloatVal);
    }
    case Parameter::Kind::GLOBAL: {
      if (GlobalVal.Offset) {
        return SymbolOffsetExpr::Create(GlobalVal.Symbol, GlobalVal.Offset);
      } else {
        return GlobalVal.Symbol;
      }
    }
  }
  llvm_unreachable("invalid parameter kind");
}

// -----------------------------------------------------------------------------
size_t SpecialisePass::ParameterHash::operator()(const Parameter &param) const
{
  switch (param.K) {
    case Parameter::Kind::INT: {
      return param.IntVal.getSExtValue();
    }
    case Parameter::Kind::FLOAT: {
      return param.FloatVal.convertToDouble();
    }
    case Parameter::Kind::GLOBAL: {
      size_t hash = 0;
      hash_combine(hash, param.GlobalVal.Symbol);
      hash_combine(hash, param.GlobalVal.Offset);
      return hash;
    }
  }
  llvm_unreachable("invalid parameter kind");
}

// -----------------------------------------------------------------------------
size_t SpecialisePass::ParametersHash::operator()(const Parameters &params) const
{
  size_t hash = 0;
  for (auto &[i, param] : params) {
    hash_combine(hash, std::hash<unsigned>{}(i));
    hash_combine(hash, ParameterHash{}(param));
  }
  return hash;
}
