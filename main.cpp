// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cstdlib>
#include "core/parser.h"
#include "core/context.h"


// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  Context ctx;
  Parser parser(ctx, argv[1]);
  parser.Parse();
  return EXIT_SUCCESS;
}
