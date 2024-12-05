#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/external_semaphore.h
               ${CMAKE_CURRENT_SOURCE_DIR}/external_semaphore_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/external_semaphore_imp.cpp
)

