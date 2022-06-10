#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueue_cmdlist.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueue_cmdlist_2.cpp
)

add_subdirectories()
