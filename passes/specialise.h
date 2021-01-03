// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/pass.h"

class Func;
class CallSite;



/**
 * Pass to eliminate unnecessary moves.
 */
class SpecialisePass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  SpecialisePass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Forward declaration of the clone class.
  class SpecialiseClone;

  /// Class to capture a constant.
  struct Parameter {
    enum class Kind {
      INT,
      FLOAT,
      GLOBAL
    };

    struct G {
      Global *Symbol;
      int64_t Offset;

      G(const G &that) : Symbol(that.Symbol), Offset(that.Offset) {}
      G(Global *symbol, int64_t offset) : Symbol(symbol), Offset(offset) {}

      bool operator==(const G &that) const
      {
        return Symbol == that.Symbol && Offset == that.Offset;
      }
    };

    Parameter(const Parameter &that);
    Parameter(const APInt &v) : K(Kind::INT), IntVal(v) {}
    Parameter(const APFloat &v) : K(Kind::FLOAT), FloatVal(v) {}
    Parameter(Global *g, int64_t i) : K(Kind::GLOBAL), GlobalVal(g, i) {}
    ~Parameter();

    bool operator==(const Parameter &that) const;

    Parameter &operator=(const Parameter &that);

    Value *ToValue() const;

    Kind K;

    union {
      APInt IntVal;
      APFloat FloatVal;
      G GlobalVal;
    };
  };

  /// Helper types to capture specialisation parameters.
  using Parameters = std::map<unsigned, Parameter>;

  /// Hasher for a parameter.
  struct ParameterHash {
    size_t operator()(const Parameter &param) const;
  };
  /// Hasher for parameters.
  struct ParametersHash {
    size_t operator()(const Parameters &param) const;
  };

  /// Specialise a function with a given set of parameters.
  void Specialise(
      Func *func,
      const Parameters &params,
      const std::set<CallSite *> &callSites
  );
  /// Specialises a function, given some parameters.
  Func *Specialise(Func *oldFunc, const Parameters &params);
  /// Specialises a call site.
  std::pair<std::vector<Ref<Inst>>, std::vector<TypeFlag>>
  Specialise(CallSite *inst, const Parameters &params);
};
