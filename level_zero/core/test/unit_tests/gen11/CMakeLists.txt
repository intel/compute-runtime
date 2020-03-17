#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_ULT_GEN11}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
endif()