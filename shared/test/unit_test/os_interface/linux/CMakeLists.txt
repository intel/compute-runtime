#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ccs_mode_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_bind_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_mm_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests_1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_mapper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_bindless_heap_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_os_memory_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_pci_speed_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_query_topology_upstream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_special_heap_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_system_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_version_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_TYPE}/file_logger_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/numa_library_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pci_path_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_uuid_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_linux_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/file_descriptor_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_lin.cpp
)

if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests_x86_64.cpp
  )
endif()

if(NEO_ENABLE_I915_PRELIM_DETECTION)
  list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object_tests_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_cache_info_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_debug_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_info_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_info_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_debug_surface_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_localmem_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_bind_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_default_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler_with_aub_dump_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_query_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_query_topology_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_vm_bind_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_with_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_tests_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/prelim_helper_func.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_prelim_uuid_tests.cpp
  )
  if(TESTS_DG1 AND "${BRANCH_TYPE}" STREQUAL "")
    list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock_prod_dg1.h
         ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_tests_dg1.cpp
    )
  endif()
  if(TESTS_XEHP_AND_LATER)
    list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_xehp_and_later_prelim_tests.cpp
    )
  endif()
endif()
if(TESTS_XEHP_AND_LATER)
  list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_xehp_and_later_tests.cpp
  )
endif()

if("${BRANCH_TYPE}" STREQUAL "")
  list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_debug_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_info_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_query_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_tests_upstream.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_localmem_upstream_tests.cpp
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_TESTS_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_LINUX})

if(UNIX)
  target_sources(neo_shared_tests PRIVATE ${NEO_CORE_OS_INTERFACE_TESTS_LINUX})
endif()

add_subdirectories()
