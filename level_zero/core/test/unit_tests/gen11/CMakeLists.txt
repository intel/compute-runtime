#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_thread_arbitration_policy_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_gen11.cpp
  )
endif()
