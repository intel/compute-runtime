#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(NEO_SHARED_TESTS_PVC
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ar_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/device_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_walker_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/product_config_tests_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_pvc.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_SHARED_TESTS_PVC})
  add_subdirectories()
endif()
