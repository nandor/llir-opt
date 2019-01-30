# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(passes
    dead_code_elim.cpp
    move_elim.cpp
    higher_order.cpp
    inliner.cpp
    sccp.cpp
    simplify_cfg.cpp
)
