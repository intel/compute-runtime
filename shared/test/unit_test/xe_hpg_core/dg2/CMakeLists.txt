#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  set(IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core_dg2.cpp)
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/ail_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ar_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_helper_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_dg2.cpp
  )

  add_subdirectories()
endif()