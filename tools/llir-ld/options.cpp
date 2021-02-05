// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include "options.h"



// -----------------------------------------------------------------------------
namespace {
using namespace llvm;
using namespace llvm::opt;

#define PREFIX(NAME, VALUE) const char *const NAME[] = VALUE;
#include "LdOptions.inc"
#undef PREFIX

const opt::OptTable::Info kOptionInfo[] =
{
  #define OPTION(X1, X2, ID, KIND, GROUP, ALIAS, X7, X8, X9, X10, X11, X12)    \
  {X1, X2, X10,         X11,         OPT_##ID, opt::Option::KIND##Class,       \
   X9, X8, OPT_##GROUP, OPT_##ALIAS, X7,       X12},
  #include "LdOptions.inc"
  #undef OPTION
};

}

// -----------------------------------------------------------------------------
OptionTable::OptionTable()
  : OptTable(kOptionInfo)
{
}

// -----------------------------------------------------------------------------
llvm::Expected<llvm::opt::InputArgList>
OptionTable::Parse(llvm::ArrayRef<const char *> argv)
{
  unsigned missingIndex;
  unsigned missingCount;
  SmallVector<const char *, 256> vec(argv.data(), argv.data() + argv.size());

  opt::InputArgList args = this->ParseArgs(vec, missingIndex, missingCount);
  if (missingCount) {
    return llvm::make_error<llvm::StringError>(
        llvm::Twine(args.getArgString(missingIndex)) + ": missing argument",
        llvm::inconvertibleErrorCode()
    );
  }

  for (auto *arg : args.filtered(OPT_UNKNOWN)) {
    std::string nearest;
    if (findNearest(arg->getAsString(args), nearest) > 1) {
      return llvm::make_error<llvm::StringError>(
          "'" + arg->getAsString(args) + "'",
          llvm::inconvertibleErrorCode()
      );
    } else {
      return llvm::make_error<llvm::StringError>(
          "'" + arg->getAsString(args) + "', did you mean '" + nearest + "'",
          llvm::inconvertibleErrorCode()
      );
    }
  }

  return args;
}
