#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  append_sources_from_properties(HW_SOURCES_GEN11 L0_HW_SOURCES_GEN11)

  target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_ULT_GEN11}
    ${HW_SOURCES_GEN11}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    )
endif()