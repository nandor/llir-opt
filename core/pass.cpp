// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/pass.h"
#include "core/pass_manager.h"



// -----------------------------------------------------------------------------
Pass::Pass(PassManager *passManager)
  : passManager_(passManager)
{
}

// -----------------------------------------------------------------------------
Pass::~Pass()
{
}

// -----------------------------------------------------------------------------
const PassConfig &Pass::GetConfig() const
{
  return passManager_->GetConfig();
}
