#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLS)
  set(IGDRCL_SRCS_tests_gen12lp_adls_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_adls.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_adls_excludes})

  set(IGDRCL_SRCS_tests_gen12lp_adls
      ${IGDRCL_SRCS_tests_gen12lp_adls_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adls.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_adls})
  add_subdirectories()
endif()
