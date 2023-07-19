#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_os_interface_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_wddm_memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_os_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_tests_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_handler_tests.cpp
)

if(WIN32)
  target_sources(igdrcl_tests PRIVATE
                 ${IGDRCL_SRCS_tests_os_interface_windows}
                 ${IGDRCL_SRC_tests_wddm_interface}
  )
endif()

add_subdirectories()
