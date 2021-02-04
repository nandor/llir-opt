// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Endian.h>

class Prog;



/// Magic number for LLIR bitcode files.
constexpr uint32_t kLLIRMagic = 0x52494C4C;
/// Returns true if the buffer contains and LLIR object.
bool IsLLIRObject(llvm::StringRef buffer);


/**
 * Helper to map the number of bytes to an integer.
 */
template<int N>
struct sized_uint {};
template<> struct sized_uint<1> { typedef uint8_t type; };
template<> struct sized_uint<2> { typedef uint16_t type; };
template<> struct sized_uint<4> { typedef uint32_t type; };
template<> struct sized_uint<8> { typedef uint64_t type; };


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
std::string ParseToolName(llvm::StringRef argv0, const char *tool);
