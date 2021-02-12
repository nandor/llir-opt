// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/cast.h"
#include "core/insts/call.h"
#include "core/insts/control.h"
#include "core/insts/mov.h"
#include "core/insts/phi.h"

#define GET_CAST_INTF
#include "instructions.def"

#define GET_CLASS_INTF
#include "instructions.def"
