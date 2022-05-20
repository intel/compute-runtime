#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_SDV)
  set(NEO_CORE_TESTS_XE_HP_CORE_XEHP
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xehp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xehp.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_CORE_TESTS_XE_HP_CORE_XEHP})
endif()
