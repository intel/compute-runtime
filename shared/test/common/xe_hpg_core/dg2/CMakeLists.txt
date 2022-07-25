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

  target_sources(${TARGET_NAME} PRIVATE
                 ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_configs_dg2.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_dg2.h

  )

  add_subdirectories()
endif()
