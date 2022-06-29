#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(IGDRCL_SRCS_tests_gen12lp_tgllp_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_tgllp.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_tgllp_excludes})

  set(IGDRCL_SRCS_tests_gen12lp_tgllp
      ${IGDRCL_SRCS_tests_gen12lp_tgllp_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_tgllp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_helper_tgllp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_tgllp.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_tgllp})
  add_subdirectories()
endif()
