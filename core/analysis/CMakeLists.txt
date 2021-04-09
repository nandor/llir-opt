# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(analysis
    call_graph.cpp
    dominator.cpp
    live_variables.cpp
    loop_nesting.cpp
    object_graph.cpp
    reference_graph.cpp
    union_find.cpp
)
add_dependencies(analysis core)
