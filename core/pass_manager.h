// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ArrayRef.h>

#include <type_traits>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <set>

#include "core/pass.h"
#include "core/analysis.h"

class Pass;
class Target;



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
  /// Enable the verifier.
  bool Verify = false;
  /// Name of the entry point.
  std::string Entry;

  PassConfig() {}

  PassConfig(
      OptLevel opt,
      bool isStatic,
      bool isShared,
      bool verify,
      std::string entry)
    : Opt(opt)
    , Static(isStatic)
    , Shared(isShared)
    , Verify(verify)
    , Entry(entry)
  {
  }
};


/**
 * Pass manager, scheduling passes.
 */
class PassManager final {
public:
  PassManager(
      const PassConfig &config,
      const Target *target,
      bool verbose,
      bool time)
    : config_(config)
    , target_(target)
    , verbose_(verbose)
    , time_(time)
  {
  }

  /// Add an analysis into the pipeline.
  template<typename T, typename... Args>
  void Add(const Args &... args)
  {
    if constexpr (std::is_base_of<Analysis, T>::value) {
      groups_.emplace_back(std::make_unique<T>(this), &AnalysisID<T>::ID);
    } else {
      groups_.emplace_back(std::make_unique<T>(this, args...), nullptr);
    }
  }

  /// Adds a group of passes to the pipeline.
  template<typename... Ts>
  void Group()
  {
    std::vector<PassInfo> passes;
    ((passes.emplace_back(std::make_unique<Ts>(this), nullptr)), ...);
    groups_.emplace_back(std::move(passes));
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
  /// Returns a reference to the target.
  const Target *GetTarget() const { return target_; }

private:
  /// Description of a pass.
  struct PassInfo {
    /// Instance of the pass.
    std::unique_ptr<Pass> P;
    /// ID to save the pass results under.
    const char *ID;

    PassInfo(std::unique_ptr<Pass> &&pass, const char *id)
      : P(std::move(pass))
      , ID(id)
    {
    }
  };

  /// Runs and measures a single pass.
  bool Run(PassInfo &pass, Prog &prog);

  /// Description of a pass group.
  struct GroupInfo {
    /// Passes in the group.
    std::vector<PassInfo> Passes;
    /// Flag to indicate whether group repeats until convergence.
    bool Repeat;

    GroupInfo(std::unique_ptr<Pass> &&pass, const char * id)
      : Repeat(false)
    {
      Passes.emplace_back(std::move(pass), id);
    }

    GroupInfo(std::vector<PassInfo> &&passes)
      : Passes(std::move(passes))
      , Repeat(true)
    {
    }
  };

  /// Configuration.
  PassConfig config_;
  /// Underlying target.
  const Target *target_;
  /// Verbosity flag.
  bool verbose_;
  /// Timing flag.
  bool time_;
  /// List of passes to run on a program.
  std::vector<GroupInfo> groups_;
  /// Mapping from named passes to IDs.
  std::unordered_map<const char *, Pass *> analyses_;
  /// Mapping from pass names to their running times.
  std::unordered_map<const char *, std::vector<double>> times_;
};



// -----------------------------------------------------------------------------
template<typename T> T* Pass::getAnalysis()
{
  return passManager_->getAnalysis<T>();
}
