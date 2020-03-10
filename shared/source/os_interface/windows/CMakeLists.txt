#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(KMDAF_HEADERS_DIR)
  set(KMDAF_FILE_SUFFIX "")
else()
  set(KMDAF_FILE_SUFFIX "_stub")
endif()

set(NEO_CORE_OS_INTERFACE_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_additional_context_flags.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_engine_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/environment_variables.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener${KMDAF_FILE_SUFFIX}.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_manager_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls.h
  ${CMAKE_CURRENT_SOURCE_DIR}/thk_wrapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_defs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_operations_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_operations_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_allocations_container.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_allocations_container.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_residency_controller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_defs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_wrapper.h
)

if(WIN32)
  set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_WINDOWS ${NEO_CORE_OS_INTERFACE_WINDOWS})
endif()
