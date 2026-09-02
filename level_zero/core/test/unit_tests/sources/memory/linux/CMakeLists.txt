#
# Copyright (C) 2022-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_ipc_handle_properties.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_ipc_mem_handle_range.cpp
  )
endif()