#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_os_interface_linux
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/device_os_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests_1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests_2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_drm_memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linux_create_command_queue_with_properties_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_linux.h
    ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux_tests.cpp
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  if(TESTS_XE_HPC_CORE)
    list(APPEND IGDRCL_SRCS_tests_os_interface_linux
         ${CMAKE_CURRENT_SOURCE_DIR}/cl_mem_cache_clos_tests_xe_hpc.cpp
    )
  endif()

endif()

if(NEO__LIBVA_FOUND)
  list(APPEND IGDRCL_SRCS_tests_os_interface_linux
       ${CMAKE_CURRENT_SOURCE_DIR}/drm_va_sharing_tests.cpp
  )
endif()

if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_os_interface_linux})
endif()
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_os_interface_linux ${IGDRCL_SRCS_tests_os_interface_linux})
add_subdirectories()
