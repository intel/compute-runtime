#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_GTPIN
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/pin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pin.h
)

set_property(GLOBAL PROPERTY NEO_GTPIN ${NEO_GTPIN})
add_subdirectories()
