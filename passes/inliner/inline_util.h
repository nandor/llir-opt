// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once


class Func;


/**
 * Returns true if inlining callee into caller is lega.
 */
bool CanInline(const Func *caller, const Func *callee);
