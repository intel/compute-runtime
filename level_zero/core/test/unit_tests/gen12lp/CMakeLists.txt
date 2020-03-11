#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  append_sources_from_properties(HW_SOURCES_GEN12LP L0_HW_SOURCES_GEN12LP)

  target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_ULT_GEN12LP}
    ${HW_SOURCES_GEN12LP}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  add_subdirectoriesL0(${CMAKE_CURRENT_SOURCE_DIR} "*")
endif()