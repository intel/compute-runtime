#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN9}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_l0_mocks_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_gen9.cpp
  )
endif()
