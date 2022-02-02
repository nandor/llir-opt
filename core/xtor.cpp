// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/xtor.h"

#include "core/cast.h"
#include "core/func.h"
#include "core/global.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Xtor::Xtor(int priority, Global *g, Kind k)
  : priority_(priority)
  , func_(std::make_shared<Use>(g, nullptr))
  , kind_(k)
{
}

// -----------------------------------------------------------------------------
Func *Xtor::GetFunc() const
{
  return ::cast<Func>(func_->get()).Get();
}

// -----------------------------------------------------------------------------
void Xtor::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Xtor::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}
