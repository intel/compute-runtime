#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(KMDAF_HEADERS_DIR AND WIN32)
  set(KMDAF_FILE_SUFFIX "")
else()
  set(KMDAF_FILE_SUFFIX "_stub")
endif()

set(NEO_CORE_OS_INTERFACE_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_windows.h
    ${CMAKE_CURRENT_SOURCE_DIR}/environment_variables.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_drm_stub.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_properties_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_metrics_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_socket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table_manager_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/trim_callback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory_create_with_fallback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory_dxgi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory_dxgi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/read_preemption_regkey.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_inc.cpp
)

set(NEO_CORE_OS_INTERFACE_WDDM
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}wddm_additional_context_flags.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}wddm_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}wddm_apply_additional_map_gpu_va_fields.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}wddm_engine_mapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3dkmthk_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dxgi_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_time_gpu_cpu_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_time_wddm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_time_wddm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_windows.h
    ${CMAKE_CURRENT_SOURCE_DIR}/environment_variables.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_wddm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/init_wddm_os_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener${KMDAF_FILE_SUFFIX}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kmdaf_listener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/max_chunk_size_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_environment_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_environment_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sharedata_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls.h
    ${CMAKE_CURRENT_SOURCE_DIR}/thk_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory_dxcore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory_dxcore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/configure_device_address_space_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/set_gmm_input_args_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/max_mem_alloc_size_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/helper_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_defs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/wddm_residency_logger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}wddm_memory_manager_allocate_in_device_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/um_km_data_translator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/um_km_data_translator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm/create_um_km_data_translator.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls_wrapper.h
)

if(NOT WIN32 AND NOT DISABLE_WDDM_LINUX)
  list(APPEND NEO_CORE_OS_INTERFACE_WDDM
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm/adapter_factory_create_dxcore.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/trim_callback_stub.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls_wrapper_drm_or_wddm.cpp
  )
endif()

add_subdirectories()

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_WINDOWS ${NEO_CORE_OS_INTERFACE_WINDOWS})
set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_WDDM ${NEO_CORE_OS_INTERFACE_WDDM})
