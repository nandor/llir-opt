// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "timeout.h"



// -----------------------------------------------------------------------------
Timeout::Timeout(unsigned seconds)
  : seconds_(seconds)
  , end_(std::chrono::system_clock::now() + std::chrono::seconds(seconds))
{
}

// -----------------------------------------------------------------------------
Timeout::operator bool () const
{
  return seconds_ && end_ <= std::chrono::system_clock::now();
}
