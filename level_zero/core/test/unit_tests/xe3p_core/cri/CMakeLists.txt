#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_CRI)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_excludes_cri.cpp
  )
endif()
