#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLS)
  set(NEO_CORE_TESTS_GEN12LP_ADLS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_adls.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_CORE_TESTS_GEN12LP_ADLS})
  add_subdirectories()
endif()
