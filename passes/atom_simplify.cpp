// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/atom_simplify.h"



// -----------------------------------------------------------------------------
const char *AtomSimplifyPass::kPassID = "atom-simplify";

// -----------------------------------------------------------------------------
static bool CoalescibleAlignment(
    std::optional<llvm::Align> base,
    llvm::Align align)
{
  return base && align < *base;
}

// -----------------------------------------------------------------------------
bool AtomSimplifyPass::Run(Prog &prog)
{
  bool changed = false;
  for (Data &data : prog.data()) {
    for (Object &object : data) {
      for (auto at = object.begin(); at != object.end(); ) {
        Atom *base = &*at++;
        uint64_t offset = base->GetByteSize();
        while (at != object.end()) {
          Atom *next = &*at++;
          if (!next->IsLocal()) {
            break;
          }

          if (auto align = next->GetAlignment()) {
            if (!CoalescibleAlignment(base->GetAlignment(), *align)) {
              break;
            }
            if (unsigned pad = llvm::offsetToAlignment(offset, *align)) {
              base->AddItem(new Item(Item::Space{ pad }));
              offset += pad;
            }
            next->SetAlignment(llvm::Align(1));
          }

          uint64_t size = next->GetByteSize();
          SymbolOffsetExpr *newExpr = nullptr;
          for (auto ut = next->use_begin(); ut != next->use_end(); ) {
            Use &use = *ut++;
            if (auto *expr = ::cast_or_null<Expr>(use.getUser())) {
              switch (expr->GetKind()) {
                case Expr::Kind::SYMBOL_OFFSET: {
                  auto *symExpr = static_cast<SymbolOffsetExpr *>(expr);
                  SymbolOffsetExpr *exprOffset;
                  if (symExpr->GetOffset() == 0) {
                    if (!newExpr) {
                      newExpr = SymbolOffsetExpr::Create(base, offset);
                    }
                    exprOffset = newExpr;
                  } else {
                    exprOffset = SymbolOffsetExpr::Create(
                        base,
                        symExpr->GetOffset() + offset
                    );
                  }
                  for (auto et = expr->use_begin(); et != expr->use_end(); ) {
                    *et++ = exprOffset;
                  }
                  assert(expr->use_empty() && "uses of expression remaining");
                  delete expr;
                  continue;
                }
              }
              llvm_unreachable("invalid expression kind");
            } else {
              if (!newExpr) {
                newExpr = SymbolOffsetExpr::Create(base, offset);
              }
              use = newExpr;
            }
          }
          for (auto it = next->begin(); it != next->end(); ) {
            Item *item = &*it++;
            item->removeFromParent();
            base->AddItem(item);
          }
          assert(next->use_empty() && "uses of atom remaining");
          next->eraseFromParent();
          offset += size;
        }
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *AtomSimplifyPass::GetPassName() const
{
  return "Atom simplification";
}
