// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/func.h"
#include "passes/inliner/inline_util.h"


// -----------------------------------------------------------------------------
static bool HasNonLocalBlocks(const Func *callee)
{
  for (const Block &block : *callee) {
    if (!block.IsLocal()) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
static bool HasAlloca(const Func *callee)
{
  for (const Block &block : *callee) {
    for (const Inst &inst : block) {
      if (inst.Is(Inst::Kind::ALLOCA)) {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool CanInline(const Func *caller, const Func *callee)
{
  // Do not inline certain functions.
  switch (callee->GetCallingConv()) {
    case CallingConv::C:
      break;
    case CallingConv::CAML:
      break;
    case CallingConv::CAML_GC:
    case CallingConv::CAML_ALLOC:
    case CallingConv::SETJMP:
    case CallingConv::XEN:
    case CallingConv::INTR:
      return false;
  }

  if (callee == caller || callee->IsNoInline() || callee->IsVarArg()) {
    // Definitely do not inline recursive, noinline and vararg calls.
    return false;
  }
  if (HasNonLocalBlocks(callee)) {
    // Do not inline the function if unique copies of the blocks are needed.
    return false;
  }
  if (HasAlloca(callee) && caller->GetCallingConv() == CallingConv::CAML) {
    // Do not inline alloca into OCaml callees.
    return false;
  }
  return true;
}
