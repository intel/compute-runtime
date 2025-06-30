#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clos_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clos_cache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/device_time_drm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_time_drm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_debug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_bind.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_bind.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_default.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_default.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_with_aub_dump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_create_multi_host_debug_surface_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_wrappers_checks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_wrappers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_wrappers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external_semaphore_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_drm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_descriptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_properties_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linux_inc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/i915.h
    ${CMAKE_CURRENT_SOURCE_DIR}/i915_prelim.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_i915.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_prelim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_getter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_upstream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}memory_info_populate_tile_to_lmem_region_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/numa_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numa_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_metrics_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_socket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pci_path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pci_path.h
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table_manager_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/settings_reader_create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pmt_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pmt_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/system_info_extended.cpp
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_uuid_xehp_and_later.inl
  )
endif()

if(SUPPORT_MTL_AND_LATER)
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_mtl_and_later.inl)
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_xe_hpc_and_later.inl)
endif()

if(DISABLE_WDDM_LINUX)
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_wddm_stub.cpp
  )
else()
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm_linux/compatible_driver_store.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm_linux/definitions/gdi_interface_logging.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm_linux/sys_calls_win.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm_linux/wddm_interface_extra.cpp
  )
endif()

if(NOT NEO_ENABLE_I915_PRELIM_DETECTION)
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_getter_upstream.cpp
  )
  list(REMOVE_ITEM NEO_CORE_OS_INTERFACE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_getter.cpp
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX})
add_subdirectories()
