#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_bindless_heaps_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compiler_interface_spirv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compiler_interface_spirv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_diagnostic_collector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_elf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_partition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_partition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_physical_address_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.h
)

if(WIN32)
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_wddm_direct_submission.h
  )
else()
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.h
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_tests_mocks ${NEO_CORE_tests_mocks})
