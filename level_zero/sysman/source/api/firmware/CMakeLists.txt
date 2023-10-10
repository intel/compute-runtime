#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_firmware.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_firmware.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_firmware_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_firmware_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_firmware.h
)

add_subdirectories()
