#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueue_cmdlist.cpp
)
add_subdirectories()
