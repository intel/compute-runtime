#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLS)
  set(IGDRCL_SRCS_tests_gen12lp_adls
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adls.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_adls})
  add_subdirectories()
endif()
