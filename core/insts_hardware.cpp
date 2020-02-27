// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst.h"
#include "core/insts_hardware.h"



// -----------------------------------------------------------------------------
RdtscInst::RdtscInst(Type type, const AnnotSet &annot)
  : OperatorInst(Inst::Kind::RDTSC, type, 0, annot)
{
}
