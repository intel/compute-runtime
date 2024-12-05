#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_SEMAPHORE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/external_semaphore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external_semaphore.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_SEMAPHORE ${NEO_CORE_SEMAPHORE})
add_subdirectories()
