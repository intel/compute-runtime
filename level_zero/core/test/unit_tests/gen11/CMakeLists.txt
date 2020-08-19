#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN11}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_l0_mocks_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_thread_arbitration_policy_gen11.cpp
  )
endif()
