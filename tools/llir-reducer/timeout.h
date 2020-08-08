// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <chrono>



/**
 * Timeout test class.
 */
class Timeout {
public:
  /// Initialise the timeout to now + seconds.
  Timeout(unsigned seconds);

  /// Returns true after timeout.
  operator bool () const;

private:
  /// Seconds to time out after.
  unsigned seconds_;
  /// End time point.
  std::chrono::system_clock::time_point end_;
};
