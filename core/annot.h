// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <stdint.h>



/**
 * Allowed annotations.
 */
enum Annot {
  CAML_CALL_FRAME  = (1 << 0),
  CAML_RAISE_FRAME = (1 << 1),
  CAML_ROOT_FRAME  = (1 << 2),
  CAML_VALUE       = (1 << 3),
};


/**
 * Class representing a set of annotations.
 */
class AnnotSet final {
public:
  /// Creats a new annotation set.
  AnnotSet() : annots_(0ull) {}

  /// Destroys the annotation set.
  ~AnnotSet();

  /// Checks if an annotation is set.
  bool Has(Annot annot) const
  {
    return annots_ & (1 << annot);
  }

  /// Sets an annotation.
  void Set(Annot annot)
  {
    annots_ |= 1 << annot;
  }

  /// Clears an annotation.
  void Clear(Annot annot)
  {
    annots_ &= ~(1 << annot);
  }

  /// Checks if any annotations are set.
  operator bool () const
  {
    return annots_ != 0;
  }

private:
  uint64_t annots_;
};
