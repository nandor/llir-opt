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
  , args_(call->GetNumArgs())
  , stack_(0)
{
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseCall(const CallSite *call)
{
  // Handle fixed args.
  auto it = call->arg_begin();
  for (unsigned i = 0, nargs = call->GetNumArgs(); i < nargs; ++i, ++it) {
    Assign(i, (*it)->GetType(0), *it);
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
    Assign(i, params[i], args[i]);
  }
}

// -----------------------------------------------------------------------------
void CallLowering::Assign(unsigned i, Type type, const Inst *value)
{
  switch (conv_) {
    case CallingConv::C:          return AssignC(i, type, value);
    case CallingConv::SETJMP:     return AssignC(i, type, value);
    case CallingConv::CAML:       return AssignOCaml(i, type, value);
    case CallingConv::CAML_ALLOC: return AssignOCamlAlloc(i, type, value);
    case CallingConv::CAML_GC:    return AssignOCamlGc(i, type, value);
    case CallingConv::CAML_RAISE: return AssignC(i, type, value);
  }
  llvm_unreachable("invalid calling convention");
}
