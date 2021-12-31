#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  add_subdirectories()

  set(neo_libult_common_SRCS_LIB_ULT_SPECIAL_HELPER
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/special_ult_helper_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/special_ult_helper_gen12lp.h
  )

  set_property(GLOBAL PROPERTY neo_libult_common_SRCS_LIB_ULT_SPECIAL_HELPER ${neo_libult_common_SRCS_LIB_ULT_SPECIAL_HELPER})
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_SPECIAL_HELPER})
endif()
