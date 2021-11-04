#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  add_subdirectories()

  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/special_ult_helper_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/special_ult_helper_gen12lp.h
  )
endif()
