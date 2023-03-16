#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/power.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/power.h
               ${CMAKE_CURRENT_SOURCE_DIR}/power_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/power_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_power.h
)

add_subdirectories()
