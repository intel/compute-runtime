#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

list(APPEND NEO_CORE_UTILITIES_LINUX
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_LINUX ${NEO_CORE_UTILITIES_LINUX})
