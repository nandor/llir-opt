// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cstdlib>
#include "core/parser.h"



// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  Parser parser(argv[1]);
  parser.Parse();
  return EXIT_SUCCESS;
}
