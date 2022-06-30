#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_SDV)
  set(NEO_CORE_TESTS_XE_HP_CORE_XE_HP_SDV
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xe_hp_sdv.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hp_sdv.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_CORE_TESTS_XE_HP_CORE_XE_HP_SDV})

  add_subdirectories()
endif()
