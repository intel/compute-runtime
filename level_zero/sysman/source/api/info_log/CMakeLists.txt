#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_info_log.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_info_log.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_info_log_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_info_log_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_info_log.h
)

add_subdirectories()
