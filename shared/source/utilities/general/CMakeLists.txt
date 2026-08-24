#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
set_property(GLOBAL APPEND PROPERTY NEO_CORE_UTILITIES
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info_general.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cpuintrinsics.cpp
)
