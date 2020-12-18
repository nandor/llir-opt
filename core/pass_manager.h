// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <type_traits>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

#include "core/pass.h"
#include "core/analysis.h"

class Pass;



/**
 * Enumeration of optimisation levels.
 */
enum class OptLevel {
  /// No optimisations.
  O0,
  /// Simple optimisations.
  O1,
  /// Aggressive optimisations.
  O2,
  /// Slow optimisations.
  O3,
  /// All optimisations.
  O4,
  /// Optimise for size.
  Os,
};

/**
 * Pass manager configuration.
 */
struct PassConfig {
  /// Optimisation level.
  OptLevel Opt = OptLevel::O0;
  /// Building a static executable.
  bool Static = false;
  /// Building a shared library.
  bool Shared = false;
  /// Name of the entry point.
  std::string Entry;

  PassConfig(OptLevel opt, bool isStatic, bool isShared, std::string entry)
    : Opt(opt)
    , Static(isStatic)
    , Shared(isShared)
    , Entry(entry)
  {
  }
};


/**
 * Pass manager, scheduling passes.
 */
class PassManager final {
public:
  PassManager(const PassConfig &config, bool verbose, bool time);

  /// Add an analysis into the pipeline.
  template<typename T>
  typename std::enable_if<std::is_base_of<Analysis, T>::value>::type Add()
  {
    passes_.push_back({new T(this), {&AnalysisID<T>::ID}});
  }

  /// Add a pass to the pipeline.
  template<typename T, typename... Args>
  typename std::enable_if<!std::is_base_of<Analysis, T>::value>::type
  Add(const Args &... args)
  {
    passes_.emplace_back(new T(this, args...), std::nullopt);
  }

  /// Adds a pass to the pipeline.
  void Add(Pass *pass);
  /// Runs the pipeline.
  void Run(Prog &prog);

  /// Returns an available analysis.
  template<typename T> T* getAnalysis()
  {
    if (auto it = analyses_.find(&AnalysisID<T>::ID); it != analyses_.end()) {
      return static_cast<T *>(it->second);
    } else {
      return nullptr;
    }
  }

  /// Returns a reference to the configuration.
  const PassConfig &GetConfig() const { return config_; }

private:
  /// Configuration.
  PassConfig config_;
  /// Verbosity flag.
  bool verbose_;
  /// Timing flag.
  bool time_;
  /// List of passes to run on a program.
  std::vector<std::pair<std::unique_ptr<Pass>, std::optional<char *>>> passes_;
  /// Mapping from named passes to IDs.
  std::unordered_map<char *, Pass *> analyses_;
};



// -----------------------------------------------------------------------------
template<typename T> T* Pass::getAnalysis()
{
  return passManager_->getAnalysis<T>();
}
