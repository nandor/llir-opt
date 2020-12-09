// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_printer.h"



// -----------------------------------------------------------------------------
void GetPrinterWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_PRINTER\n";



  OS << "#undef GET_PRINTER\n";
  OS << "#endif // GET_PRINTER\n";
}
