#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_CORE)
  set(IGDRCL_SRCS_tests_xe_hp_core
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${NEO_SOURCE_DIR}/shared/test/common/xe_hp_core/unit_test_helper_xe_hp_core.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hp_core})
  add_subdirectories()
endif()
