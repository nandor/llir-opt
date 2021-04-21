// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/Support/CommandLine.h>

#include "core/cast.h"
#include "core/func.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/global_forward.h"
#include "passes/global_forward/nodes.h"
#include "passes/global_forward/forwarder.h"

#define DEBUG_TYPE "global-forward"



// -----------------------------------------------------------------------------
const char *GlobalForwardPass::kPassID = "global-forward";

// -----------------------------------------------------------------------------
const char *GlobalForwardPass::GetPassName() const
{
  return "Global Load/Store Forwarding";
}

// -----------------------------------------------------------------------------
bool GlobalForwardPass::Run(Prog &prog)
{
  auto &cfg = GetConfig();
  const std::string start = cfg.Entry.empty() ? "_start" : cfg.Entry;
  auto *entry = ::cast_or_null<Func>(prog.GetGlobal(start));
  if (!entry) {
    return false;
  }

  GlobalForwarder forwarder(prog, *entry);

  bool changed = false;
  changed = forwarder.Forward() || changed;
  changed = forwarder.Reverse() || changed;
  return changed;
}
