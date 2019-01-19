#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(KMDAF_HEADERS_DIR)
  set(KMDAF_FILE_SUFFIX "")
else()
  set(KMDAF_FILE_SUFFIX "_stub")
endif()

set(RUNTIME_SRCS_OS_INTERFACE_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d10_11_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d9_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/environment_variables.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener${KMDAF_FILE_SUFFIX}.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls.h
  ${CMAKE_CURRENT_SOURCE_DIR}/thk_wrapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_32bit_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_additional_context_flags.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_engine_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_controller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_defs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_wrapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_interface.cpp
)

if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE
    ${RUNTIME_SRCS_OS_INTERFACE_WINDOWS}
  )
endif()

add_subdirectories()
