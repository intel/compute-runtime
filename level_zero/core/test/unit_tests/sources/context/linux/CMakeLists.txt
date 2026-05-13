#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}test_context_helper_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_init_opaque_handle_resources.cpp
  )
endif()

add_subdirectories()

