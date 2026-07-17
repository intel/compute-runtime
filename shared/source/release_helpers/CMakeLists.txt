#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_RELEASE_HELPERS_DEFINITIONS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}release_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}supported_num_grfs.h
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_RELEASE_HELPER ${NEO_CORE_RELEASE_HELPERS_DEFINITIONS})

add_subdirectories()
