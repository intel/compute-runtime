#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_ZEINFO_EXTRA
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}zeinfo_decoder_ext.cpp
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_ZEINFO_EXTRA ${NEO_CORE_ZEINFO_EXTRA})

add_subdirectories()
