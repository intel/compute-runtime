#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_gen12lp_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe_hpg_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe_lpg_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe_hpc_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe2_hpg_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe3_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe3p_tests.cpp
)

add_subdirectories()
