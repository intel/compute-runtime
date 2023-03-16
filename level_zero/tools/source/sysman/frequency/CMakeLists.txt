#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/frequency.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/frequency.h
               ${CMAKE_CURRENT_SOURCE_DIR}/frequency_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/frequency_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_frequency.h
)

add_subdirectories()
