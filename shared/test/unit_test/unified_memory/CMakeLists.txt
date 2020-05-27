#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT SKIP_NEO_UNIT_TESTS AND NOT SKIP_UNIT_TESTS)

  target_sources(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_tests.cpp
  )

endif()
