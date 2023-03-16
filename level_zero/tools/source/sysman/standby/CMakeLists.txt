#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/os_standby.h
               ${CMAKE_CURRENT_SOURCE_DIR}/standby.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/standby.h
               ${CMAKE_CURRENT_SOURCE_DIR}/standby_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/standby_imp.h
)

add_subdirectories()
