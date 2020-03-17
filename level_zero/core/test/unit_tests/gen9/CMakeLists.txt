#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_ULT_GEN9}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
endif()
