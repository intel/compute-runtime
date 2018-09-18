#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_os_interface_base
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/32bit_memory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_gen_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_os_interface_base})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_os_interface_base ${IGDRCL_SRCS_tests_os_interface_base})
add_subdirectories()
