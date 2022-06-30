#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  set(NEO_SHARED_aub_tests_configurations
      ${NEO_SHARED_aub_tests_configurations}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration_gen11.cpp
  )

  set(NEO_SHARED_aub_tests_configurations ${NEO_SHARED_aub_tests_configurations} PARENT_SCOPE)
endif()
