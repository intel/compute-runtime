#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  set(NEO_CORE_TESTS_GEN12LP_ADLN
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_adln.cpp
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP_ADLN ${NEO_CORE_TESTS_GEN12LP_ADLN})
  add_subdirectories()
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_TESTS_GEN12LP_ADLN}
  )
endif()
