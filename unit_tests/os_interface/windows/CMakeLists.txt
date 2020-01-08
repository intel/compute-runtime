#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_os_interface_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_os_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_dll_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_memory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_win_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_environment_variables.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_kmdaf_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_time_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_address_space_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_kmdaf_listener_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_mapper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_memory_manager_allocate_in_device_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_allocate_in_device_pool_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_controller_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_handler_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_registry_reader.cpp
)
if(WIN32)
  get_property(NEO_CORE_tests_os_interface_windows GLOBAL PROPERTY NEO_CORE_tests_os_interface_windows)
  list(APPEND IGDRCL_SRCS_tests_os_interface_windows ${NEO_CORE_tests_os_interface_windows})

  file(GLOB IGDRCL_SRC_tests_wddm_interface "${CMAKE_CURRENT_SOURCE_DIR}/wddm2[0-9]_tests\.cpp")
  target_sources(igdrcl_tests PRIVATE
    ${IGDRCL_SRCS_tests_os_interface_windows}
    ${IGDRCL_SRC_tests_wddm_interface}
  )
endif()
add_subdirectories()
