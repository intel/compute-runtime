#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/os_temperature.h
               ${CMAKE_CURRENT_SOURCE_DIR}/temperature.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/temperature.h
               ${CMAKE_CURRENT_SOURCE_DIR}/temperature_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/temperature_imp.h
)

add_subdirectories()
