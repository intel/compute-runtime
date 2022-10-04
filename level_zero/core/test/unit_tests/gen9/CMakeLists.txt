#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_range_barrier_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_create_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueuecommandlist_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_hw_helper_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_gen9.cpp
  )
endif()
