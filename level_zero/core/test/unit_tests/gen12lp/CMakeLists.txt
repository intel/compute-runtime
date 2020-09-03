#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN12LP}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_l0_mocks_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_events_gen12lp.cpp
  )

  target_include_directories(${TARGET_NAME} PRIVATE ${COMPUTE_RUNTIME_DIR}/level_zero/core/source/gen12lp/definitions${BRANCH_DIR_SUFFIX}/)
endif()
