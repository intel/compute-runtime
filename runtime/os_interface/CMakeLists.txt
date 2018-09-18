#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/32bit_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/DebugVariables_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/DebugVariables.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.inl  
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_BASE ${RUNTIME_SRCS_OS_INTERFACE_BASE})
add_subdirectories()
