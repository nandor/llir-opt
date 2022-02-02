// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "emitter/call_lowering.h"

#include "core/insts.h"
#include "core/func.h"



// -----------------------------------------------------------------------------
CallLowering::CallLowering(const Func *func)
  : conv_(func->GetCallingConv())
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const CallSite *call)
  : conv_(call->GetCallingConv())
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const RaiseInst *inst)
  : conv_(*inst->GetCallingConv())
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const LandingPadInst *inst)
  : conv_(*inst->GetCallingConv())
{
}

// -----------------------------------------------------------------------------
CallLowering::CallLowering(const ReturnInst *inst)
  : conv_(inst->getParent()->getParent()->GetCallingConv())
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
    AssignArg(i, FlaggedType((*it).GetType(), call->flag(i)));
  }

  // Handle arguments.
  for (unsigned i = 0, ntypes = call->type_size(); i < ntypes; ++i) {
    // TODO: attach flags to return values.
    AssignRet(i, call->type(i));
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseFunc(const Func *func)
{
  const auto &params = func->params();
  for (unsigned i = 0, n = params.size(); i < n; ++i) {
    AssignArg(i, params[i]);
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseReturn(const ReturnInst *inst)
{
  // Handle returned values.
  for (unsigned i = 0, nargs = inst->arg_size(); i < nargs; ++i) {
    AssignRet(i, inst->arg(i).GetType());
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalyseRaise(const RaiseInst *inst)
{
  // Handle returned values.
  for (unsigned i = 0, nargs = inst->arg_size(); i < nargs; ++i) {
    AssignRet(i, inst->arg(i).GetType());
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AnalysePad(const LandingPadInst *inst)
{
  // Handle returned values.
  for (unsigned i = 0, nargs = inst->type_size(); i < nargs; ++i) {
    AssignRet(i, inst->type(i));
  }
}

// -----------------------------------------------------------------------------
void CallLowering::AssignArg(unsigned i, FlaggedType type)
{
  switch (conv_) {
    case CallingConv::C:          return AssignArgC(i, type);
    case CallingConv::SETJMP:     return AssignArgC(i, type);
    case CallingConv::CAML:       return AssignArgOCaml(i, type);
    case CallingConv::CAML_ALLOC: return AssignArgOCamlAlloc(i, type);
    case CallingConv::CAML_GC:    return AssignArgOCamlGc(i, type);
    case CallingConv::XEN:        return AssignArgXen(i, type);
    case CallingConv::INTR:       llvm_unreachable("no arguments to interrupt");
    case CallingConv::MULTIBOOT:  return AssignArgMultiboot(i, type);
    case CallingConv::WIN64:      return AssignArgWin64(i, type);
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
void CallLowering::AssignRet(unsigned i, FlaggedType type)
{
  switch (conv_) {
    case CallingConv::C:          return AssignRetC(i, type);
    case CallingConv::SETJMP:     return AssignRetC(i, type);
    case CallingConv::CAML:       return AssignRetOCaml(i, type);
    case CallingConv::CAML_ALLOC: return AssignRetOCamlAlloc(i, type);
    case CallingConv::CAML_GC:    return AssignRetOCamlGc(i, type);
    case CallingConv::XEN:        return AssignRetXen(i, type);
    case CallingConv::INTR:       llvm_unreachable("no returns from interrupt");
    case CallingConv::MULTIBOOT:  llvm_unreachable("no returns from multiboot");
    case CallingConv::WIN64:      return AssignRetWin64(i, type);
  }
  llvm_unreachable("invalid calling convention");
}
