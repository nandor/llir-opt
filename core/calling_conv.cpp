// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/calling_conv.h"



// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, CallingConv conv)
{
  switch (conv) {
    case CallingConv::C:          return os << "c";
    case CallingConv::CAML:       return os << "caml";
    case CallingConv::CAML_ALLOC: return os << "caml_alloc";
    case CallingConv::CAML_GC:    return os << "caml_gc";
    case CallingConv::SETJMP:     return os << "setjmp";
    case CallingConv::XEN:        return os << "xen";
    case CallingConv::INTR:       return os << "intr";
    case CallingConv::MULTIBOOT:  return os << "multiboot";
    case CallingConv::WIN64:      return os << "win64";
  }
  llvm_unreachable("invalid calling convention");
}
