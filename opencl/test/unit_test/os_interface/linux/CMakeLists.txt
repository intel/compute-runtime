#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_os_interface_linux
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_os_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_mm_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests_1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests_2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_mapper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}drm_memory_manager_localmem_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_os_memory_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_residency_handler_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_system_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_uuid_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_linux_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/linux_create_command_queue_with_properties_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_time_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_lin.cpp
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND IGDRCL_SRCS_tests_os_interface_linux
       ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_fixture_context.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_fixture_context.h
       ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_fixture_prelim.h
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object_tests_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_debug_surface_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_prelim_fixtures.h
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_localmem_prelim_tests.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_residency_handler_prelim_tests.cpp
  )
endif()

if(NEO__LIBVA_FOUND)
  list(APPEND IGDRCL_SRCS_tests_os_interface_linux
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_va_sharing_tests.cpp
  )
endif()

if(TESTS_XEHP_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_os_interface_linux
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_xehp_and_later_tests.cpp
  )
endif()

if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_os_interface_linux})
endif()
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_os_interface_linux ${IGDRCL_SRCS_tests_os_interface_linux})
add_subdirectories()
