#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/os_ras.h
               ${CMAKE_CURRENT_SOURCE_DIR}/ras.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ras.h
               ${CMAKE_CURRENT_SOURCE_DIR}/ras_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ras_imp.h
)

add_subdirectories()
