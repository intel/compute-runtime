#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_debugger.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueue_cmdlist.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueue_cmdlist_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_priority_extension.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_sip_residency.cpp
)

add_subdirectories()
