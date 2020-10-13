// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>

class Prog;



/**
 * Helper to load data from a buffer.
 */
template<typename T> T ReadData(llvm::StringRef buffer, uint64_t offset)
{
  namespace endian = llvm::support::endian;
  if (offset + sizeof(T) > buffer.size()) {
    llvm::report_fatal_error("invalid bitcode file");
  }

  auto *data = buffer.data() + offset;
  return endian::read<T, llvm::support::little, 1>(data);
}


/**
 * Parses an object or a bitcode file.
 */
std::unique_ptr<Prog> Parse(llvm::StringRef buffer, std::string_view name);

/**
 * Converts a path to an absolute path.
 */
std::string Abspath(const std::string &path);

/**
 * Extract a triple from the tool name.
 */
std::string ParseToolName(const std::string &argv0, const char *tool);

/**
 * Create a tool name including a triple.
 */
std::string CreateToolName(llvm::StringRef triple, const char *tool);
