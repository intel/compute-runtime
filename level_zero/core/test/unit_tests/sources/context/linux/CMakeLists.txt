#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}test_context_helper_linux.cpp
  )
endif()

add_subdirectories()

