#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/engine.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/engine.h
               ${CMAKE_CURRENT_SOURCE_DIR}/engine_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/engine_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_engine.h
)

add_subdirectories()
