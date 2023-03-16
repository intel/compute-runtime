#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_imp.h
)

add_subdirectories()

