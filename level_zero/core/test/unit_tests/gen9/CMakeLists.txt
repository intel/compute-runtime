#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  append_sources_from_properties(HW_SOURCES_GEN9 L0_HW_SOURCES_GEN9)

  target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_ULT_GEN9}
    ${HW_SOURCES_GEN9}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

    add_subdirectoriesL0(${CMAKE_CURRENT_SOURCE_DIR} "*")
endif()
