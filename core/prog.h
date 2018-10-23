// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>

class Data;
class Func;



class Prog {
public:
  /// Creates a new program.
  Prog();

  /// Adds a function to the program.
  Func *AddFunc(const std::string &str);

  // Fetch data segments.
  Data *GetData() const { return data_; }
  Data *GetBSS() const { return bss_; }
  Data *GetConst() const { return const_; }

private:
  /// .data segment
  Data *data_;
  /// .bss segment
  Data *bss_;
  /// .const segment
  Data *const_;
};
