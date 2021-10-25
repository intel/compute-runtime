#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GLOBAL_FACTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/create_os_context_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_os_time_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/discover_devices_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/init_os_interface_${DRIVER_MODEL}.cpp
)

set(NEO_CORE_OS_INTERFACE
    ${NEO_CORE_GLOBAL_FACTORIES}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_receiver_${DRIVER_MODEL}/create_command_stream_receiver.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_bdw_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_environment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters.h
    ${CMAKE_CURRENT_SOURCE_DIR}/print.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_inc_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls_common.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_OS_INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_xehp_and_later.inl)
endif()

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE ${NEO_CORE_OS_INTERFACE})
add_subdirectories()
