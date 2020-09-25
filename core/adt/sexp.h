// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <memory>
#include <vector>

#include <llvm/Support/raw_ostream.h>



/**
 * S-Expression base.
 */
class SExp {
public:
  enum class Kind {
    NUMBER,
    STRING,
    LIST,
  };

  /// Storage for numbers.
  class Number {
  public:
    Number(int64_t v) : k_(Kind::NUMBER), v_(v) {}

    /// Returns the value.
    int64_t Get() const { return v_; }

    /// Print the s-expression.
    void print(llvm::raw_ostream &os) const;

  private:
    /// Value kind.
    Kind k_;
    /// Underlying number value.
    int64_t v_;
  };

  /// Storage for strings.
  class String {
  public:
    String(const std::string &v) : k_(Kind::STRING), v_(v) {}

    /// Returns the value.
    const std::string &Get() const { return v_; }

    /// Print the s-expression.
    void print(llvm::raw_ostream &os) const;

  private:
    /// Value kind.
    Kind k_;
    /// Underlying string value.
    std::string v_;
  };

  /// Storage for lists.
  class List {
  public:
    List() : k_(Kind::LIST) {}

    Number *AddNumber(int64_t v);
    String *AddString(const std::string &v);
    List *AddList();

    size_t size() const { return v_.size(); }
    const SExp &operator[](size_t idx) const { return v_[idx]; }

    /// Print the s-expression.
    void print(llvm::raw_ostream &os) const;

  private:
    /// Value kind.
    Kind k_;
    /// Underlying list value.
    std::vector<SExp> v_;
  };

public:
  SExp() : s_(std::make_unique<Storage>()) {}
  SExp(int64_t v) : s_(std::make_unique<Storage>(v)) {}
  SExp(const std::string &v) : s_(std::make_unique<Storage>(v)) {}

  SExp(SExp &&that) : s_(std::move(that.s_)) {}
  SExp(const SExp &that) : s_(std::make_unique<Storage>(*that.s_)) {}
  ~SExp();

  /// Returns the node as a number or nullptr.
  const Number *AsNumber() const;
  /// Returns the node as a number or nullptr.
  Number *AsNumber();
  /// Returns the node as a string or nullptr.
  const String *AsString() const;
  /// Returns the node as a string or nullptr.
  String *AsString();
  /// Returns the node as a list or nullptr.
  const List *AsList() const;
  /// Returns the node as a list or nullptr.
  List *AsList();

  /// Print the s-expression.
  void print(llvm::raw_ostream &os) const;

private:
  /// Union of storage kinds.
  union Storage {
    /// Universal access to kind.
    Kind K;
    /// Number storage.
    Number N;
    /// String storage.
    String S;
    /// List storage.
    List L;

    Storage() { new (&L) List(); }
    Storage(int64_t v) { new (&N) Number(v); }
    Storage(const std::string &v) { new (&S) String(v); }
    Storage(const Storage &that);
    ~Storage();
  };

  /// Underlying storage.
  std::unique_ptr<Storage> s_;
};
