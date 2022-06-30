#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_RKL)
  set(NEO_SHARED_tests_gen12lp_rkl
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen12lp_rkl.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_SHARED_tests_gen12lp_rkl})

  add_subdirectories()
endif()
