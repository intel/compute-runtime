#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_CAPS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/caps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/materialize_caps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}apply_macro_for_all_future_releases.h
    ${CMAKE_CURRENT_SOURCE_DIR}/apply_macro_for_all_releases.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}caps_all.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_gen12lp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe_hpg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe_lpg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe_hpc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe2_hpg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_xe3p.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_setup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/caps_setup.cpp
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_COMPILER_RELEASE_HELPER ${NEO_CORE_CAPS})

add_subdirectories()
