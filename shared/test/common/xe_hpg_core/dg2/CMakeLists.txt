#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  set(IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_dg2.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes})

  set(NEO_CORE_TESTS_XE_HPG_CORE_DG2
      ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/product_configs_dg2.h
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_helper_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_dg2.h
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_CORE_TESTS_XE_HPG_CORE_DG2})
  add_subdirectories()
endif()
