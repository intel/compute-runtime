#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_tests_compiler_mocks
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_cif.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_cif.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.h
)

set_property(GLOBAL PROPERTY NEO_CORE_tests_compiler_mocks ${NEO_CORE_tests_compiler_mocks})

set(NEO_CORE_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_center.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_file_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_bindless_heaps_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cif.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cif.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compiler_interface_spirv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compiler_interface_spirv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compilers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compilers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_diagnostic_collector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_elf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_execution_environment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_experimental_command_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_partition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_partition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_client_context_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_client_context_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_io_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_migration_sync_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_multi_graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_physical_address_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_tbx_csr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_tbx_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mock_gmm_client_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mock_gmm_client_context.h
)

if(WIN32)
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_interface20.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_residency_allocations_container.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_residency_logger.h
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm_mock_helpers.h
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_wddm_direct_submission.h
  )
else()
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.h
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_tests_mocks ${NEO_CORE_tests_mocks})
