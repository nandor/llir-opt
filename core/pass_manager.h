// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <type_traits>
#include <unordered_map>
#include <vector>

#include "core/pass.h"
#include "core/analysis.h"

class Pass;



/**
 * Pass manager, scheduling passes.
 */
class PassManager final {
public:
  PassManager(bool verbose, bool time);


  /// Add an analysis into the pipeline.
  template<typename T>
  typename std::enable_if<std::is_base_of<Analysis, T>::value>::type Add()
  {
    passes_.push_back({new T(this), {&AnalysisID<T>::ID}});
  }

  /// Add a pass to the pipeline.
  template<typename T>
  typename std::enable_if<!std::is_base_of<Analysis, T>::value>::type Add()
  {
    passes_.emplace_back(new T(this), std::nullopt);
  }

  /// Adds a pass to the pipeline.
  void Add(Pass *pass);
  /// Runs the pipeline.
  void Run(Prog *prog);

  /// Returns an available analysis.
  template<typename T> T* getAnalysis()
  {
    if (auto it = analyses_.find(&AnalysisID<T>::ID); it != analyses_.end()) {
      return static_cast<T *>(it->second);
    } else {
      return nullptr;
    }
  }

private:
  /// Verbosity flag.
  bool verbose_;
  /// Timing flag.
  bool time_;
  /// List of passes to run on a program.
  std::vector<std::pair<Pass *, std::optional<char *>>> passes_;
  /// Mapping from named passes to IDs.
  std::unordered_map<char *, Pass *> analyses_;
};



// -----------------------------------------------------------------------------
template<typename T> T* Pass::getAnalysis()
{
  return passManager_->getAnalysis<T>();
}
