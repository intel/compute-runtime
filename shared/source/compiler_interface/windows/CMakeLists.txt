#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMPILER_INTERFACE_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_compiler_cache_helper.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_COMPILER_INTERFACE_WINDOWS ${NEO_CORE_COMPILER_INTERFACE_WINDOWS})
