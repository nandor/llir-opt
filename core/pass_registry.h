// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <llvm/Support/ErrorHandling.h>
#include "core/pass.h"
#include "core/pass_manager.h"

class PassManager;



/**
 * Pass registry which tracks all the available passes and analyses.
 */
class PassRegistry {
public:
  ~PassRegistry();

  /// Register a pass with the registry.
  template <typename T>
  void Register()
  {
    auto it = registry_.emplace(T::kPassID, nullptr);
    if (!it.second) {
      llvm::report_fatal_error("Pass already registered");
    }
    it.first->second = std::make_unique<Registrar<T>>();
  }

  /// Add a pass to the manager.
  void Add(PassManager &mngr, const std::string &name) {
    auto it = registry_.find(name);
    if (it == registry_.end()) {
      llvm::report_fatal_error("Pass not found: '" + name + "'");
    }
    it->second->Add(mngr);
  }

private:
  struct RegistrarBase {
    virtual ~RegistrarBase();

    virtual void Add(PassManager &mngr) = 0;
  };

  template <typename T>
  struct Registrar : public RegistrarBase {
    void Add(PassManager &mngr) override {
      mngr.Add<T>();
    }
  };

  /// Mapping from names to registrars.
  std::unordered_map<std::string, std::unique_ptr<RegistrarBase>> registry_;
};
