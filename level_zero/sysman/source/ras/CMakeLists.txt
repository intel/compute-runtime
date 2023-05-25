#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_ras.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_ras_imp.h
)

add_subdirectories()
