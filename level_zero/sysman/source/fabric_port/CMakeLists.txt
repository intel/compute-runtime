#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port.h
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_fabric_port.h
)

add_subdirectories()
