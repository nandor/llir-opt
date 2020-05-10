// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <tuple>
#include <vector>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>

class Block;
class Data;
class Expr;
class Extern;
class Func;
class Global;
class Inst;
class PhiInst;
class Prog;
class Value;



/// Magic number for bitcode files.
constexpr uint32_t kBitcodeMagic = 0x52494C4C;

/**
 * Helper class to deserialise a program from a binary format.
 */
class BitcodeReader final {
public:
  BitcodeReader(llvm::StringRef buf) : buf_(buf), offset_(0) {}

  /// Read a program from the stream.
  std::unique_ptr<Prog> Read();

private:
  /// Read a data segment.
  void Read(Func &func);
  /// Read a function.
  void Read(Data &data);

  /// Write a primitive to the file.
  template<typename T> T ReadData();
  /// Emit a string.
  std::string ReadString();
  /// Read an instruction.
  Inst *ReadInst(
      const std::vector<Inst *> &map,
      std::vector<std::tuple<PhiInst *, Block *, unsigned>> &fixups
  );
  /// Reads an expression.
  Expr *ReadExpr();
  /// Reads a value.
  Value *ReadValue(const std::vector<Inst *> &map);

private:
  /// Buffer to read from.
  llvm::StringRef buf_;
  /// Offset into the buffer.
  uint64_t offset_;
  /// Mapping from offsets to globals.
  std::vector<Global *> globals_;
};


/**
 * Helper class to serialise the program into a binary format.
 */
class BitcodeWriter final {
public:
  BitcodeWriter(llvm::raw_pwrite_stream &os) : os_(os) {}

  /// Write a program to the stream.
  void Write(const Prog &prog);

private:
  /// Write a function to the stream.
  void Write(const Func &prog);
  /// Write a data item to the stream.
  void Write(const Data &data);
  /// Writes an instruction to the stream.
  void Write(
      const Inst &inst,
      const std::unordered_map<const Inst *, unsigned> &map
  );
  /// Writes an expression.
  void Write(const Expr *expr);

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
