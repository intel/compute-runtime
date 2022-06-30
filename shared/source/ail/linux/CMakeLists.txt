#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AIL_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_configuration_linux.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_AIL_LINUX ${NEO_CORE_AIL_LINUX})
add_subdirectories()
