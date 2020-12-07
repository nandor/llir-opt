// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <tuple>
#include <vector>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>

#include "core/inst.h"
#include "core/util.h"

class Block;
class Data;
class Expr;
class Extern;
class Func;
class Global;
class PhiInst;
class Prog;
class Value;
class Object;
class Atom;
class Annot;
class AnnotSet;
class Xtor;



/// Magic number for LLIR bitcode files.
constexpr uint32_t kLLIRMagic = 0x52494C4C;
/// Magic number for LLAR bitcode files.
constexpr uint32_t kLLARMagic = 0x52414C4C;

/// Returns true if the buffer contains and LLIR object.
bool IsLLIRObject(llvm::StringRef buffer);
/// Returns true if the buffer contains and LLAR object.
bool IsLLARArchive(llvm::StringRef buffer);



/**
 * Helper class to deserialise a program from a binary format.
 */
class BitcodeReader final {
public:
  BitcodeReader(llvm::StringRef buf) : buf_(buf), offset_(0) {}

  /// Read a program from the stream.
  std::unique_ptr<Prog> Read();

private:
  /// Read a function.
  void Read(Func &func);
  /// Read an atom.
  void Read(Atom &atom);
  /// Read an extern.
  void Read(Extern &ext);

  /// Read a primitive from the file.
  template<typename T> T ReadData();
  /// Read an optional value from the file.
  template<typename T> std::optional<T> ReadOptional();
  /// Emit a string.
  std::string ReadString();
  /// Read an instruction.
  Inst *ReadInst(
      const std::vector<Ref<Inst>> &map,
      std::vector<std::tuple<PhiInst *, Block *, unsigned>> &fixups
  );
  /// Reads an expression.
  Expr *ReadExpr();
  /// Reads a value.
  Ref<Value> ReadValue(const std::vector<Ref<Inst>> &map);
  /// Reads an annotation.
  void ReadAnnot(AnnotSet &annots);
  /// Reads a constructor/destructor.
  Xtor *ReadXtor();
  /// Reads a flagged type.
  FlaggedType ReadFlaggedType();

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
  /// Write an atom to the stream.
  void Write(const Atom &atom);
  /// Writes an extern to the stream.
  void Write(const Extern &ext);
  /// Writes an instruction to the stream.
  void Write(
      const Inst &inst,
      const std::unordered_map<ConstRef<Inst>, unsigned> &map
  );
  /// Writes an expression.
  void Write(const Expr &expr);
  /// Writes an annotation.
  void Write(const Annot &annot);
  /// Writes a constructor/destructor.
  void Write(const Xtor &xtor);
  /// Writes a flagged type.
  void Write(const FlaggedType &type);

  /// Emit a string ref.
  void Emit(llvm::StringRef str);
  /// Emit a C++ string.
  void Emit(const std::string &str) { return Emit(llvm::StringRef(str)); }
  /// Write a primitive to the file.
  template<typename T> void Emit(T t);

private:
  /// Mapping from symbols to IDs.
  std::unordered_map<const Global *, unsigned> symbols_;
  /// Stream to write to.
  llvm::raw_pwrite_stream &os_;
};
