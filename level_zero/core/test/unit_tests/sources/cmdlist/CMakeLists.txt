#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_4.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_5.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_api.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_barrier.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_event_reset.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_memory.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_signal_event.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_wait_on_events.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_blit.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_fill.cpp
)
add_subdirectories()
