#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_UTILITIES_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/directory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timer_util.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_LINUX ${NEO_CORE_UTILITIES_LINUX})

add_subdirectories()
