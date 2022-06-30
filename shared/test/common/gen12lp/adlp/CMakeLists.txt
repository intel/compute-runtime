#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLP)
  set(NEO_CORE_TESTS_GEN12LP_ADLP
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/preamble_helper_tests_adlp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_adlp.cpp
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP_ADLP ${NEO_CORE_TESTS_GEN12LP_ADLP})
  add_subdirectories()
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_TESTS_GEN12LP_ADLP}
  )
endif()
