#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_power.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_power.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_power_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_power_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_power.h
)

add_subdirectories()

