#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AIL_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_configuration_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/ail_ov_comp_wa_windows.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_AIL_WINDOWS ${NEO_CORE_AIL_WINDOWS})
add_subdirectories()
