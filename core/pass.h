// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

class Prog;
class PassManager;
class PassConfig;
class Target;



/**
 * Abstract base class for program-altering passes.
 */
class Pass {
public:
  /**
   * Pass initialisation.
   */
  Pass(PassManager *passManager);

  /**
   * Pass cleanup.
   */
  virtual ~Pass();

  /**
   * Runs the pass on a program.
   */
  virtual bool Run(Prog &prog) = 0;

  /**
   * Returns the name of the pass.
   */
  virtual const char *GetPassName() const = 0;

  /// Returns an available analysis.
  template<typename T> T* getAnalysis();


protected:
  /// Returns a reference to the configuration.
  const PassConfig &GetConfig() const;
  /// Returns a reference to the target.
  const Target *GetTarget() const;

protected:
  /// Pass manager scheduling this pass.
  PassManager *passManager_;
};
