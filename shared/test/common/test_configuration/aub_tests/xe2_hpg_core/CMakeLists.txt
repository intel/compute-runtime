#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE2_HPG_CORE)
  set(NEO_SHARED_aub_tests_configurations
      ${NEO_SHARED_aub_tests_configurations}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration_xe2_hpg_core.cpp
  )

  set(NEO_SHARED_aub_tests_configurations ${NEO_SHARED_aub_tests_configurations} PARENT_SCOPE)
endif()
