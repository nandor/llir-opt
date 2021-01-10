# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(analysis
    dominator.cpp
    live_variables.cpp
    loop_nesting.cpp
    union_find.cpp
    reference_graph.cpp
)
add_dependencies(analysis core)
