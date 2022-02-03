// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/TableGen/Error.h>
#include <llvm/TableGen/Main.h>
#include <llvm/TableGen/Record.h>

#include "get_bitcode.h"
#include "get_cast.h"
#include "get_class.h"
#include "get_clone.h"
#include "get_compare.h"
#include "get_instruction.h"
#include "get_printer.h"
#include "get_parser.h"


// -----------------------------------------------------------------------------
bool LLIRTableGenMain(llvm::raw_ostream &os, llvm::RecordKeeper &records)
{
  GetBitcodeWriter(records).run(os);
  GetClassWriter(records).run(os);
  GetCloneWriter(records).run(os);
  GetCompareWriter(records).run(os);
  GetInstructionWriter(records).run(os);
  GetPrinterWriter(records).run(os);
  GetCastWriter(records).run(os);
  GetParserWriter(records).run(os);
  return false;
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::cl::ParseCommandLineOptions(argc, argv);
  llvm::llvm_shutdown_obj Y;
  return TableGenMain(argv[0], &LLIRTableGenMain);
}

#ifdef __has_feature
#if __has_feature(address_sanitizer)
#include <sanitizer/lsan_interface.h>
int __lsan_is_turned_off() { return 1; }
#endif
#endif
