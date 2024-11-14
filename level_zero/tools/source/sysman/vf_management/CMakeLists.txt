#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/os_vf.h
               ${CMAKE_CURRENT_SOURCE_DIR}/vf_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/vf_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/vf_management.h
               ${CMAKE_CURRENT_SOURCE_DIR}/vf_management.cpp
)

add_subdirectories()
