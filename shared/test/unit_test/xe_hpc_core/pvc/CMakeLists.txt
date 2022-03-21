#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(NEO_SHARED_TESTS_PVC
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_walker_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_pvc.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_SHARED_TESTS_PVC})
  add_subdirectories()
endif()
