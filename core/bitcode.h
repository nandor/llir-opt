// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>

class Data;
class Extern;
class Func;
class Global;
class Prog;



/**
 * Helper class to deserialise a program from a binary format.
 */
class BitcodeReader final {

};


/**
 * Helper class to serialise the program into a binary format.
 */
class BitcodeWriter final {
public:
  BitcodeWriter(llvm::raw_pwrite_stream &os) : os_(os) {}

  /// Write a program to the stream.
  void Write(const Prog *prog);

private:
  /// Write a function to the stream.
  void Write(const Func &prog);
  /// Write a data item to the stream.
  void Write(const Data &data);

  /// Emit a string.
  void Emit(llvm::StringRef str);
  /// Write a primitive to the file.
  template<typename T> void Emit(T t);

private:
  /// Mapping from symbols to IDs.
  std::unordered_map<const Global *, unsigned> symbols_;
  /// Stream to write to.
  llvm::raw_pwrite_stream &os_;
};
