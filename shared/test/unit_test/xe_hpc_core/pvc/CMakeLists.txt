#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ar_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_walker_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpc_core_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_pvc.cpp
  )

  add_subdirectories()
endif()