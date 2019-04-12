// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/analysis/dominator.h"


namespace llvm {
namespace DomTreeBuilder {
template void Calculate<BlockDomTree>(BlockDomTree &DT);
}
}

