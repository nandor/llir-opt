# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(stats
    alloc_size.cpp
)
add_dependencies(stats llir-core)
