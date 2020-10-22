// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"
#include "emitter/call_lowering.h"



// -----------------------------------------------------------------------------
CallLowering::CallLowering(const Func *func)
  : conv_(func->GetCallingConv())
  , args_(func->params().size())
  , stack_(0)
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const CallSite *call)
  : conv_(call->GetCallingConv())
  , args_(call->arg_size())
  , rets_(call->type_size())
  , stack_(0)
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const RaiseInst *inst)
  : conv_(inst->getParent()->getParent()->GetCallingConv())
  , args_(0)
  , rets_(inst->arg_size())
  , stack_(0)
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const ReturnInst *inst)
  : conv_(inst->getParent()->getParent()->GetCallingConv())
  , args_(0)
  , rets_(inst->arg_size())
  , stack_(0)
{
}

// -----------------------------------------------------------------------------
CallLowering::~CallLowering()
{
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseCall(const CallSite *call)
{
  // Handle fixed args.
  auto it = call->arg_begin();
  for (unsigned i = 0, nargs = call->arg_size(); i < nargs; ++i, ++it) {
    AssignArg(i, (*it)->GetType(0), *it);
  }

  // Handle arguments.
  for (unsigned i = 0, ntypes = call->type_size(); i < ntypes; ++i) {
    AssignRet(i, call->type(i));
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseFunc(const Func *func)
{
  const auto &params = func->params();
  unsigned nargs = params.size();
  std::vector<const ArgInst *> args(nargs, nullptr);
  for (const Block &block : *func) {
    for (const Inst &inst : block) {
      if (inst.GetKind() != Inst::Kind::ARG) {
        continue;
      }

      auto &argInst = static_cast<const ArgInst &>(inst);
      if (argInst.GetIdx() >= nargs) {
        llvm_unreachable("Function declared fewer args");
      }
      args[argInst.GetIdx()] = &argInst;
      if (params[argInst.GetIdx()] != argInst.GetType()) {
        llvm_unreachable("Argument declared with different type");
      }
    }
  }

  for (unsigned i = 0; i < nargs; ++i) {
    AssignArg(i, params[i], args[i]);
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseReturn(const ReturnInst *inst)
{
  // Handle returned values.
  for (unsigned i = 0, nargs = inst->arg_size(); i < nargs; ++i) {
    AssignRet(i, inst->arg(i)->GetType(0));
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseRaise(const RaiseInst *inst)
{
  // Handle returned values.
  for (unsigned i = 0, nargs = inst->arg_size(); i < nargs; ++i) {
    AssignRet(i, inst->arg(i)->GetType(0));
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AssignArg(unsigned i, Type type, ConstRef<Inst> value)
{
  switch (conv_) {
    case CallingConv::C:          return AssignArgC(i, type, value);
    case CallingConv::SETJMP:     return AssignArgC(i, type, value);
    case CallingConv::CAML:       return AssignArgOCaml(i, type, value);
    case CallingConv::CAML_ALLOC: return AssignArgOCamlAlloc(i, type, value);
    case CallingConv::CAML_GC:    return AssignArgOCamlGc(i, type, value);
    case CallingConv::CAML_RAISE: return AssignArgC(i, type, value);
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
void CallLowering::AssignRet(unsigned i, Type type)
{
  switch (conv_) {
    case CallingConv::C:          return AssignRetC(i, type);
    case CallingConv::SETJMP:     return AssignRetC(i, type);
    case CallingConv::CAML:       return AssignRetOCaml(i, type);
    case CallingConv::CAML_ALLOC: return AssignRetOCamlAlloc(i, type);
    case CallingConv::CAML_GC:    return AssignRetOCamlGc(i, type);
    case CallingConv::CAML_RAISE: return AssignRetC(i, type);
  }
  llvm_unreachable("invalid calling convention");
}
