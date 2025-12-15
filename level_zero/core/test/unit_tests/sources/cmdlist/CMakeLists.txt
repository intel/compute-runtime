#
# Copyright (C) 2020-2025 Intel Corporation
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
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_6.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_7.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_8.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_api.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_barrier.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_event_reset.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_launch_kernel_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_memory.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_multipartition_prologue.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_signal_event.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_append_wait_on_events.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_blit.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_fill.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_host_functions.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_api.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_memory_extension.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_in_order_cmdlist_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_in_order_cmdlist_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_in_order_cmdlist_3.cpp
)

if(TESTS_XEHP_AND_LATER)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_copy_event_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_fill_event_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_xehp_and_later.cpp
  )
endif()

if(TESTS_XE2_AND_LATER)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_xe2_and_later.cpp
  )
endif()

add_subdirectories()
